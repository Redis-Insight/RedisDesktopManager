#include "Response.h"
#include "RedisException.h"

Response::Response()
    : responseSource(""), lastValidPos(0), itemsCount(0)
{
}

Response::Response(const QByteArray & src)
    : responseSource(src), lastValidPos(0), itemsCount(0)
{
}

Response::~Response(void)
{
}

void Response::setSource(const QByteArray& src)
{
    responseSource = src;
}

void Response::clear()
{
    responseSource.clear();
    lastValidPos = 0;
    itemsCount = 0;
}

QByteArray Response::source()
{
    return responseSource;
}

void Response::appendToSource(QString& src)
{
    responseSource.append(src);
}

void Response::appendToSource(QByteArray& src)
{
    responseSource.append(src);
}

QString Response::toString()
{
    return responseSource.left(1500);
}

QVariant Response::getValue()
{
    if (responseSource.isEmpty()) {
        return QVariant();
    }

    ResponseType t = getResponseType(responseSource);

    QVariant  parsedResponse;

    try {

        switch (t) {
        case Status:
        case Error:        
            parsedResponse = QVariant(getStringResponse(responseSource));
            break;

        case Integer:        
            parsedResponse = QVariant(getStringResponse(responseSource).toInt());
            break;

        case Bulk:
            parsedResponse = QVariant(parseBulk(responseSource));
            break;

        case MultiBulk:         
            parsedResponse = QVariant(parseMultiBulk(responseSource));        
            break;
        case Unknown:
            break;
        }

    } catch (RedisException &e) {
        parsedResponse = QVariant(QStringList() << e.what());
    }

    return parsedResponse;
}    

QString Response::parseBulk(const QByteArray& response)
{
    int endOfFirstLine = response.indexOf("\r\n");
    int responseSize = getSizeOfBulkReply(response, endOfFirstLine);    

    if (responseSize != -1) {
        return response.mid(endOfFirstLine + 2, responseSize);        
    }

    return QString();
}

QStringList Response::parseMultiBulk(const QByteArray& response)
{    
    int endOfFirstLine = response.indexOf("\r\n");
    int responseSize = getSizeOfBulkReply(response, endOfFirstLine);            

    if (responseSize == 0) 
    {    
        return QStringList();
    }

    QStringList parsedResult; ResponseType type; int firstItemLen, firstPosOfEndl, bulkLen;

    parsedResult.reserve(responseSize+5);

    for (int currPos = endOfFirstLine + 2, respStringSize = response.size(); currPos < respStringSize;) 
    {        
        type = getResponseType(response.at(currPos));

        firstPosOfEndl = response.indexOf("\r\n", currPos);
        firstItemLen = firstPosOfEndl - currPos-1;

        if (type == Integer) {                                            
            parsedResult << response.mid(currPos+1, firstItemLen);

            currPos = firstPosOfEndl + 2;
            continue;
        } else if (type == Bulk) {                                    
            bulkLen = response.mid(currPos+1, firstItemLen).toInt();

            if (bulkLen == 0) 
            {
                parsedResult << "";
                currPos = firstPosOfEndl + 4;
            } else {
                parsedResult << response.mid(firstPosOfEndl+2, bulkLen);
                currPos = firstPosOfEndl + bulkLen + 4;
            }

            continue;
        } else if (type == MultiBulk) {               
            throw RedisException("Recursive parsing of MultiBulk replies not supported");
        } else {
            break;
        }
    }            

    return parsedResult;
}

Response::ResponseType Response::getResponseType(const QByteArray & r) const
{    
    return getResponseType(r.at(0));
}

Response::ResponseType Response::getResponseType(const char typeChar) const
{    
    if (typeChar == '+') return Status; 
    if (typeChar == '-') return Error;
    if (typeChar == ':') return Integer;
    if (typeChar == '$') return Bulk;
    if (typeChar == '*') return MultiBulk;

    return Unknown;
}

QString Response::getStringResponse(const QByteArray& response)
{
    return response.mid(1, response.length() - 3);
}

bool Response::isValid()
{
    return isReplyValid(responseSource);
}

bool Response::isReplyValid(const QByteArray & responseString)
{
    if (responseString.isEmpty()) 
    {
        return false;
    }

    ResponseType type = getResponseType(responseString);

    switch (type)
    {
        case Status:
        case Error:        
        case Unknown:
        default:
            return isReplyGeneralyValid(responseString);                

        case Integer:
            return isReplyGeneralyValid(responseString) 
                && isIntReplyValid(responseString);

        case Bulk:  
            return isReplyGeneralyValid(responseString) 
                && isBulkReplyValid(responseString);        

        case MultiBulk:
            return isReplyGeneralyValid(responseString) 
                && isMultiBulkReplyValid(responseString);    
    }    
}

bool Response::isReplyGeneralyValid(const QByteArray& r)
{
    return r.endsWith("\r\n");
}

int Response::getPosOfNextItem(const QByteArray &r, int startPos = 0)
{
    if (startPos >= r.size()) {
        return -1;
    }

    ResponseType type = getResponseType(r.at(startPos));

    int endOfFirstLine = r.indexOf("\r\n", startPos);

    int responseSize;

    switch (type)
    {    
    case Integer:
        return endOfFirstLine+2;

    case Bulk:          
        responseSize = getSizeOfBulkReply(r, endOfFirstLine, startPos);

        if (responseSize == -1) {
            return endOfFirstLine+2;
        } else {
            return endOfFirstLine+responseSize+4;
        }
        break;
    default:
        return -1;
    }

}

bool Response::isIntReplyValid(const QByteArray& r)
{
    return !r.isEmpty();
}

bool Response::isBulkReplyValid(const QByteArray& r)
{            
    int endOfFirstLine = r.indexOf("\r\n");
    int responseSize = getSizeOfBulkReply(r, endOfFirstLine);

    if (responseSize == -1) {
        return true;
    }

    int actualSizeOfResponse = r.size() - endOfFirstLine - 4;

    // we need not strict check for using this method for validation multi-bulk items
    if (actualSizeOfResponse < responseSize) {
        return false;
    }

    return true;
}

bool Response::isMultiBulkReplyValid(const QByteArray& r) 
{    
    int endOfFirstLine = r.indexOf("\r\n");
    int responseSize = getSizeOfBulkReply(r, endOfFirstLine);

    if (responseSize <= 0) {
        return true;
    }
        
    //fast validation based on string size
    int minimalReplySize = responseSize * 4 + endOfFirstLine; // 4 is [type char] + [digit char] + [\r\n]

    int responseStringSize = r.size();

    if (responseStringSize < minimalReplySize) {
        return false;
    }

    //detailed validation
    int currPos = (lastValidPos > 0) ? lastValidPos : endOfFirstLine + 2;
    int lastPos = 0;

    do {

        currPos = getPosOfNextItem(r, currPos);

        if (currPos != -1) {
            lastPos = currPos;
        }

        if (currPos != -1 && currPos != responseStringSize) {
            lastValidPos = currPos;
        }

    } while (currPos != -1 && ++itemsCount);


    if (itemsCount < responseSize || (lastPos != responseStringSize)) {
        return false;
    }

    return true;    
}

int Response::getSizeOfBulkReply(const QByteArray& reply, int endOfFirstLine, int beginFrom) 
{
    if (endOfFirstLine == -1) {
        endOfFirstLine = reply.indexOf("\r\n", beginFrom);
    }

    QString strRepresentaton;
    
    for (int pos = beginFrom + 1; pos < endOfFirstLine; pos++) {
        strRepresentaton += reply.at(pos);
    }

    return strRepresentaton.toInt();        
}

QString Response::valueToString(QVariant& value)
{
    if (value.isNull()) 
    {
        return "NULL";
    } else if (value.type() == QVariant::StringList) {
        return value.toStringList().join("\r\n");
    } 

    return value.toString();
}

int Response::getLoadedItemsCount()
{
    return itemsCount;
}

bool Response::isErrorMessage() const
{
    return getResponseType(responseSource) == Error
        && responseSource.startsWith("-ERR");

}