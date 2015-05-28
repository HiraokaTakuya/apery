概要

Apery は USI プロトコルの将棋エンジンです。
USI エンジンに対応した GUI ソフトを使って下さい。
将棋所 以外で動作検証しておりませんので、将棋所 の使用を推奨します。
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

メモリに最低でも 600 MB 程度空きがあること。
64bit OS であること。


使い方

最初に、実行ファイルは apery/bin フォルダに置くことを想定しています。

将棋所での使い方のみを説明します。
将棋所を立ち上げます。

Windows の場合
Shogidokoro.exe をダブルクリックして下さい。
立ち上がらない場合は、.NET Framework が古い可能性が高いです。新しいものにして下さい。


Linux の場合
terminal を立ち上げ、mono Shogidokoro.exe とコマンドを打って下さい。
立ち上がらない場合、mono のバージョンが低いか、mono のライブラリが足りない可能性が高いです。
MonoDevelop 等をインストールすれば必要なライブラリは揃うと思います。
例として、Ubuntu の場合は sudo apt-get install MonoDevelop とコマンドを打つとインストール出来ると思います。
(最新の将棋所は Mono で動かない事もあるようです。古い将棋所を使うか、Mono を可能な限り最新にすることで動作するかも知れません。
 また、将棋所を使って人間が指した時など、駒音が鳴るときに将棋所が落ちる事があるようです。音が鳴る設定を切ってお使い下さい。)


評価関数のフォルダに、*_synthesized.bin (* には色々な文字が入ります)というファイルが無い場合、起動に非常に時間が掛かります。
まずは *_synthesized.bin を生成する必要があります。
Windows の場合は apery/bin/make_synthesized_eval.bat をダブルクリックして下さい。
Linux の場合は apery/bin/make_synthesized_eval.sh を実行して下さい。
これで数分掛かると思いますが、*_synthesized.bin が生成されます。
次回起動時には Apery を高速に起動することが出来ます。


将棋所のエンジン登録で Windows の場合は apery/bin/apery.exe (Linux の場合は apery/bin/apery) を登録して下さい。
一度、「これは USI エンジンではありません。」といったポップアップが表示されるかも知れません。
タイムアウトして登録に失敗している可能性があるので、もう一度エンジン登録してみて下さい。
それでも登録に失敗するなら、Apery が正しく動作していない可能性があります。
apery/bin/apery (Windows の場合は apery/bin/apery.exe) をダブルクリックして、usi とコマンドを打ってみて下さい。
usiok が表示されない場合は、ご利用の PC では Apery が動作しないようです。


将棋所に登録出来ましたら、後は将棋所の使い方を参照して下さい。


開発者向け注意点

Linux のディストリビューションによっては、Makefile に記述されている '-lpthread' を '-pthread' にしなければ、
実行時にエラーになってしまう場合があります。
Linux, Windows で G++ 4.8 以上のバージョンで動作確認をしています。
Clang では正しくビルド出来ているか確認出来ていません。
Visual Studio でビルドすることは現状では出来ません。
Windows でビルドする場合は、MinGW64 をお使い下さい。

評価関数の機械学習をするには
ifdef.hpp で LEARN を有効にして下さい。
apery をビルドして、
./apery l <学習用棋譜ファイル名> <使用する棋譜の数(0なら全て使うという意味)> <並列数> <最低探索深さ> <最大探索深さ>
とコマンド入力すると学習を開始します。
棋譜の形式はCSA1行形式です。詳しくは learner.hpp のコメントを参照して下さい。
棋譜はプロやfloodgate上位の棋譜70000棋譜程度、最低探索深さ 3 最大探索深さ 4 で主に学習しています。

定跡を生成するには
apery をビルドして、
./apery b <定跡用棋譜ファイル名>
とコマンド入力すると定跡生成を開始します。
棋譜の形式はCSA1行形式です。詳しくは book.cpp のコメントを参照して下さい。
定跡はfloodgate上位のソフトが上手く指した棋譜のみを抽出して生成しています。
utils/onesidebook/oneside_filter.rb で floodgate の棋譜を抽出して CSA1行形式で出力しています。