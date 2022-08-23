# Spresense2413

マイコンボード Spresense に、オープンソースの YM2413（FM音源）エミュレータ [emu2413](https://github.com/digital-sound-antiques/emu2413) を移植したものです。また、[vst2413](https://github.com/keijiro/vst2413) の音源ドライバのコードの一部を流用しました。

YM2413 は、最大発音数 9 音（または 6 音＋リズム音色）の仕様ですが、Spresense の性能制限により、現在のところ、最大発音数 3 音（リズム音色使用不可）となっています。

Arudino 向けの MIDI シールドを利用すれば、MIDI 入力を受けて音源を再生することができます。

# デモ

[![demo movie](http://img.youtube.com/vi/N-bUm6l-djM/0.jpg)](https://www.youtube.com/watch?v=N-bUm6l-djM)


# ビルド環境のセットアップ

ライブラリとして、[Sound Signal Processing Library for Spresense](https://github.com/SonySemiconductorSolutions/ssih-music) を使用しています。[チュートリアル](https://github.com/SonySemiconductorSolutions/ssih-music/blob/develop/docs/Tutorial.md) を参考に、Arudino 環境をセットアップしてください。

また、[Arudino MIDI Library](https://github.com/FortySevenEffects/arduino_midi_library) も事前にインストールしておく必要があります。

上記 2 つのライブラリをセットアップしたら、Arduino IDE で Spresense2413.ino を開き、Spresense へファームウェアを書き込んでください。

# 使い方

D4 を LOW に落とすことで、音色を変更することができます。また、D5 を LOW に落とすことで、A4 (440Hz) で発音します。

Arduino MIDI シールド等を使って、UART RX ピンに 31.25Kbps で MIDI メッセージを入力すると、Note On / Note Off に応じて発音制御を行います。それ以外のメッセージには現在対応していません。
