重要なお知らせ

開発は https://github.com/HiraokaTakuya/apery_rust に移行しましたので、
最新版の取得や、開発にご参加頂く場合は、そちらを参照して下さい。

概要

Apery は USI プロトコルの将棋エンジンです。
USI エンジンに対応した GUI ソフト (将棋所やShogiGUIなど) を使って下さい。
Apery は GNU General Public License version 3 またはそれ以降のバージョンのもとで配布されます。
主にチェスエンジンの Stockfish の実装を参考にしています。
将棋固有のデータ構造、評価関数等、Bonanza の実装を非常によく参考にしています。

ファイルの説明

・Readme.txt, このファイルです。
・Copying.txt, GNU General Public License version 3 条文です。
・src/, Apery のソースコードのフォルダです。
・utils/, Apery 開発で使用する本体以外のソフトのソースコードのフォルダです。
・bin/, 評価関数や定跡のバイナリのフォルダです。ファイルサイズが巨大なので、リポジトリ自体は分けています。


利用環境

メモリに最低でも 1.2GB 程度空きがあること。
64bit OS であること。


使い方

最初に、実行ファイルは apery/bin フォルダに置くことを想定しています。
ご自身でビルドした場合は apery/src に実行ファイルが出来ますので、適宜 apery/bin に移動させて下さい。

将棋所での使い方のみを説明します。
将棋所を立ち上げます。

Windows の場合
Shogidokoro.exe をダブルクリックして下さい。


Linux の場合
terminal を立ち上げ、mono Shogidokoro.exe とコマンドを打って下さい。
立ち上がらない場合、mono のバージョンが低いか、mono のライブラリが足りない可能性が高いです。
MonoDevelop 等をインストールすれば必要なライブラリは揃うと思います。
例として、Ubuntu の場合は sudo apt-get install monodevelop とコマンドを打つとインストール出来ると思います。
(最新の将棋所は Mono で動かない事もあるようです。古い将棋所を使うか、Mono を可能な限り最新にすることで動作するかも知れません。
 また、将棋所を使って人間が指した時など、駒音が鳴るときに将棋所が落ちる事があるようです。音が鳴る設定を切ってお使い下さい。)


将棋所のエンジン登録で Windows の場合は apery/bin/apery.exe (Linux の場合は apery/bin/apery) を登録して下さい。
登録に失敗するなら、Apery が正しく動作していない可能性があります。
その場合は、apery/bin/apery (Windows の場合は apery/bin/apery.exe) をダブルクリックして、usi とコマンドを打ってみて下さい。
usiok が表示されない場合は、ご利用の PC では Apery が動作しないようです。


将棋所に登録出来ましたら、後は将棋所の使い方を参照して下さい。


開発者向け注意点

Linux, Windows で g++-7 以上のバージョンで動作確認をしています。
Visual Studio でビルドする際は、
プラットフォームを x64 にし、
C/C++ コマンドライン に
/source-charset:utf-8 /bigobj
を記載して下さい。

評価関数の機械学習をするには
ifdef.hpp を修正して LEARN を有効にして下さい。

./apery make_teacher <対局開始局面集データ(ハフマン符号化したもの)> <出力教師データ> <スレッド数> <出力教師局面数>
とコマンドすると、強化学習用の教師データを生成します。

./apery use_teacher <教師データ> <スレッド数>
とコマンドすると、強化学習を開始します。
評価関数バイナリが上書きされるので気を付けて下さい。

定跡を生成するには
apery をビルドして、
./apery b <定跡用棋譜ファイル名>
とコマンド入力すると定跡生成を開始します。
棋譜の形式はCSA1行形式です。詳しくは book.cpp のコメントを参照して下さい。
