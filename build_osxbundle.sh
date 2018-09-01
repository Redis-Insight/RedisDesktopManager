#!/bin/sh
QTPATH=/Users/admin/Qt5.1.1/5.1.1/clang_64/bin

export PATH=$PATH:$QTPATH

echo ===========================
TAG=$1
echo Version: $TAG
echo ===========================

rm -fR ./bin/linux/release/*


echo ‘============== Build crash reporter ================’
cd ./crashreporter
qmake
make -s clean
make -s

echo ‘============== Build rdm ================’
cd ./../redis-desktop-manager/
pwd
sh ./configure
qmake
make -s clean
make -s

echo ‘============== Create release bundle ================’
cd ./../

BUNDLE_PATH=./bin/linux/release/ 
BUILD_DIR=$BUNDLE_PATH/rdm.app/Contents/
MAC_TOOL=$QTPATH/macdeployqt

cp -f ./redis-desktop-manager/Info.plist $BUILD_DIR/
cp -f ./redis-desktop-manager/rdm.icns $BUILD_DIR/Resources/

cd $BUNDLE_PATH

$MAC_TOOL rdm.app -dmg 
cp rdm.dmg redis-desktop-manager-$TAG.dmg
