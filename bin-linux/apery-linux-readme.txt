#!/bin/sh

###### Use Apery USI Shogi Engine on linux!



## 1) install gShogi (http://johncheetham.com/projects/gshogi/)
## on Debian Jessie/Strech
sudo apt-get install gpsshogi build-essential python-dev python-glade2 monodevelop  
git clone git://github.com/johncheetham/gshogi
cd gshogi
python setup.py build
sudo python setup.py install

## 1a) following general steps from this guide http://johncheetham.com/projects/gshogi/usi.html,
## add GPSshogi engine using file /usr/games/gpsusi


## 2) compile and setup Apery USI Engine
git clone https://github.com/HiraokaTakuya/apery
cd apery/src
make
mv apery ../bin-linux/
cd ../bin-linux
./apery setoption name Write_Synthesized_Eval value true 
## few minutes at 98% CPU usage for this first run, generating *_synthesized.bin files

touch apery.sh
echo -e "#\041/bin/sh\nBASEDIR=`dirname $0`\ncd $BASEDIR\n./apery" >> apery.sh
chmod +x apery.sh

## 2a) following general steps from the aforementioned guide http://johncheetham.com/projects/gshogi/usi.html
## you can add Apery to gShogi using file /PATH-TO-APERY-FOLDER/bin-linux/apery.sh
