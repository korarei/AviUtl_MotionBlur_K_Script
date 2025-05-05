# @MotionBlur_K

モーションブラー系エフェクトを集めたスクリプトになる予定．

現在使用可能なスクリプト

- ObjectMotionBlur


[ダウンロードはこちらから](https://github.com/korarei/AviUtl_MotionBlur_K_Script/releases)


## 動作要件

- [AviUtl 1.10 (これ以外のバージョンでは動作しない)](http://spring-fragrance.mints.ne.jp/aviutl/)

- [拡張編集 0.92 (これ以外のバージョンでは動作しない)](http://spring-fragrance.mints.ne.jp/aviutl/)

- [GLShaderKit v0.4.0 (これ以前のバージョンでは動作しない)](https://github.com/karoterra/aviutl-GLShaderKit)

- [Visual C++ 再頒布可能パッケージ (2015/2017/2019/2022 x86版)](https://learn.microsoft.com/ja-jp/cpp/windows/latest-supported-vc-redist)


[Rikky Module](https://hazumurhythm.com/wev/amazon/?script=rikkymodulea2Z)があればダイアログでリストや追加のチェックボックスが使用可能である．


## 導入・削除

### 導入

1.  同梱の`*.anm`，`*.dll`，`shaders`フォルダを`script`フォルダまたはその子フォルダに入れる．

    v1.0.0では`shaders`フォルダ内に`MotionBlur_K.frag`のみが存在する．


### 削除

1.  導入したものを削除する．


## 使い方

### ObjectMotionBlur

v1.0.0ではオブジェクトのX，Y，拡大率，Z軸回転のトラックバーによる移動に関してモーションブラーをかける．ただし，フレーム間は線形補間で求めている．

また，Geometryのうち，ox，oy，zoom，rzに関してはデータを保持することにより，計算で扱うことが可能である． (基本効果系エフェクトやDelayMove，全自動リリックモーションなど)

v1.0.0ではグループ制御による移動は計算対象外だが，グループ制御にかけることで範囲内のオブジェクトに対して一括でエフェクトをかけることができる． (移動量はそれぞれのオブジェクトによる)


- sAngle (シャッターアングル)

  ブラー幅 (360度で1フレーム移動量と等しい)．0度から360度が現実的な値で360度を超える値は非現実的な値である．

  360度を超える値を指定した場合，前2フレームのデータを使用する．

  最小値は`0`，最大値は`720`である．初期値は`180`で一般的な値を採用している．

- sPhase (シャッターフェーズ)

  ブラー位置．sAngleを-1倍した値を指定することで現在位置から前フレーム位置にかけてブラーがかかる．0度の場合，現在位置から後フレーム位置へブラーがかかる．

  最小値は`-360`，最大値は`360`である．初期値は`-90`と一般的な値でsAngleが`180`のとき，現在位置がブラーの中央になるようにしている．

- smpLim (サンプル数制限)

  描画精度．このスクリプトは移動量に応じた可変サンプル数を採用している．ここでは，そのサンプル数の上限値を設定する．PCの重さに関わるため適切に設定してほしい．
  
  必要サンプル数はダイアログ内の`Print Info`を有効にすることで`patch.aul`で追加されるコンソールで確認できる．

  sAngleが`360`以下のときの最小必要サンプル数は`2`，`360`を超える場合は`3`であるため，それより小さい場合はエラーが出る．

  最小値は`1`，最大値は`4096`である．初期値は`256`とやや小さい値にしている．

- pvSmpLim (プレビューのサンプル数制限)

  プレビュー時の描画制度．`0`以外の値にすることで，編集時に描画制度を下げて軽量にすることができる．出力時は`smpLim`の値になる．

  最大値は`0`，最大値は`4096`である．初期値は`0`でこの機能を無効にしている．

- Mix Original Image (元画像を合成)

  元画像を元の位置に描画する．GLSL上でのAlpha値によるmixなので本来の描画と異なる可能性がある．

  標準モーションブラーエフェクトの`残像`のようなもの．

  初期値は`OFF`

- Use Geometry (ジオメトリを使用する)

  スクリプトによる座標変化を計算に入れるかどうかを指定する．SDKに合わせてこれらデータをGeometryと呼ぶことにする．

  データは共有メモリ上に保存される．ただし，ハンドル情報はAviUtl内に保存される．

  初期値は`OFF`

- Clear Method (ジオメトリデータのクリア方法)

  保存したジオメトリのデータを削除する．

  1.  None

      削除しない．

  2.  Auto

      オブジェクトの最終フレームで削除．

  3.  All Objects

      次読み込んだとき，全てのオブジェクトのデータを削除．

  4.  Current Object

      次読み込んだとき，現在のオブジェクトのデータを削除．

  上記以外の数字を入力した場合，その絶対値をObject Indexとみなし，該当のオブジェクトのデータを削除．

  初期値は`1` (None)

- Save All Geo

  全てのフレームでデータを保存するか，0，1，2フレームと前1フレーム，前2フレームのデータのみ保存するか指定する．

  保存するGeometryは1フレームあたり合計24Byte，この共有メモリ位置を示すハンドルは4Byteである．
  
  Geometryは共有メモリ上に保存されていくためAviUtlのメモリ空間を圧迫しないが，ハンドルが圧迫していく可能性がある．

  初期値は`ON` (全てのフレームでデータを保存)

- Keep Size (サイズ固定)

  チェックを入れるとサイズを変更しない．`OFF`でブラーが見切れないようにする．

  最大画像サイズを超える場合，`patch.aul`で追加されるコンソールに警告を流す．

  初期値は`OFF`

- Calc -1F & -2F (0フレーム前の座標を仮想的に計算)

  0フレームより前を0，1，2フレームを元に計算する．ただし，等加速度直線運動とみなしている．これで，easeOut系の移動方法を指定しても0フレーム目にブラーがかかる．

  初期値は`ON`

- Reload (シェーダー再読み込み)

  シェーダーをリロードする．

  `0`のとき無効化され，それ以外の数字で有効化される．また，`boolean`を指定してもよい．

  初期値は`0`

- Print Info (情報を表示)

  `patch.aul`で追加されるコンソールに情報を表示する．

  表示される情報は以下のとおり

  - Object ID

    所謂`Object Index`．GeometryはObject IDごとに保存される．これを`Clear Geo Data`に入力すると特定のオブジェクトのみデータを削除可能．

  - Index

    所謂`obj.index`．個別オブジェクトのインデックス．

  - Required Samples

    必要なサンプル数．これを目安に`smpLim`を設定してほしい．

  - Geo Clear Method

    現在の`Clear Method`を文字で示す．

  `0`のとき無効化され，それ以外の数字で有効化される．また，`boolean`を指定してもよい．

  初期値は`0`

- Shader Folder (シェーダーの格納フォルダ)

  `*.frag`のある場所を指定する．

  初期値は`"\\shaders"`


## スクリプトからの呼ぶ

### 呼び方

`package.cpath`を編集して`require`で呼べるようにして以下のようにして呼ぶ．

```lua
local MotionBlur_K = require("MotionBlur_K")

-- 関数の呼び出し
MotionBlur_K.func_name(args)
```

### `process_object_motion_blur(shutter_angle, shutter_phase, render_sample_limit, preview_sample_limit, is_orig_img_visible, is_using_geometry_enabled, geometry_data_cleanup_method, is_saving_all_geometry_enabled, is_keeping_size_enabled, is_calc_neg1f_and_neg2f_enabled, is_reload_enabled, is_printing_info_enabled, shader_folder)`関数

`ObjectMotionBlur`の項目に記載のパラメータを入れるとObjectMotionBlurがかかる．全変数省略可能で，省略時は初期値になる．


##  ビルド方法

`.github\\workflows`内の`releaser.yml`に記載．


## LICENSE
LICENSEファイルに記載．


## Credits

###  aviutl_exedit_sdk

https://github.com/ePi5131/aviutl_exedit_sdk ([今回利用したもの](https://github.com/nazonoSAUNA/aviutl_exedit_sdk/tree/command))

---

Copyright (c) 2022
ePi All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
THIS SOFTWARE IS PROVIDED BY ePi “AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL ePi BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


### Lua 5.1.5

http://www.lua.org

---

MIT license

Copyright (C) 1994-2012 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.


## Change Log 
- **v1.0.0**
  - release
