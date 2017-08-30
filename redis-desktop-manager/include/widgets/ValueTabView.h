#pragma once

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>
#include <QtWidgets/QPlainTextEdit>
#include <QMovie>
#include <QPushButton>
#include <QSplitter>
#include <QComboBox>
#include <QGroupBox>

class ValueTabView
{
public:
	QGridLayout *gridLayout;
	QLineEdit *keyName;
	QTableView *keyValue;
	QPlainTextEdit *singleValue;
	QGroupBox *singleValueGroup;
	QComboBox * singleValueFormatterType;
	QSplitter *splitter;
	QPlainTextEdit *keyValuePlain;
	QLabel *keyNameLabel;	
	QLabel *keyValueLabel;
	QLabel *loaderLabel;
	QMovie *loader;
	QLabel * formatterLabel;

	QGridLayout * paginationGrid;
	QPushButton * previousPage;
	QPushButton * nextPage;
	QLabel * pagination;

	enum Type { ModelBased, PlainBased };

	void init(QWidget * baseController);

	void initKeyValue(ValueTabView::Type t = ModelBased);

	void setModel(QAbstractItemModel * model);

	void setPlainValue(QString &);

	ValueTabView::Type getType();

protected:
	QWidget * controller;

	void initLayout();

	void initKeyName();	

	void initPagination();
};

