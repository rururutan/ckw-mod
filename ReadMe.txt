/////////////////////////////////////////////
// 概要

  コマンドプロンプトウィンドウを使い易くしたソフトです。

  コマンドプロンプトを隠しておいて、画面表示や キー操作など
  ユーザーインターフェース部分だけを置き換えています。

  Windows10/11で動作する Win32/Win64アプリケーションです。

  オリジナルの配布元は以下になります。（リンク切れ）
  http://www.geocities.jp/cygwin_ck/

  改変版はGithubにて公開しています。
  https://github.com/ckw-mod/ckw-mod

  Unicode改変版はGithubにて公開しています。
  https://github.com/rururutan/ckw-mod/

  このソフトウェアのライセンスは GNU General Public License v2です。

////////////////////////////////////////////
// コンパイル

 cmake -S . -B build
 cmake --build build --config Release

 Microsoft Visual C++ 2022でコンパイルを確認しています。

////////////////////////////////////////////
// その他

  「Console」を参考にさせて頂きました。
  http://sourceforge.net/projects/console/

  改変版を作るにあたって多数の方のパッチを取り込ませていただきました。
  この場をお借りしてお礼申し上げます。
