人力修正処理関数郡
=====

### vector<vector<size_t>> modify_guess_image(vector<vector<size_t>> before, IplImage* image, int width, int height, int  int_div_x, int int_div_y)
人力修正処理関数の本体です。
 問題クラスを引数として取り込めていないです。すみませんm(_ _)m
 その代わり、引数の内容自体は問題クラスのメンバ変数と同じものを取り込みます。
 後ほど問題クラスを引数として取り込めるように修正します。

### void Mouse(int event, int x, int y, int flags, void* param)
マウス処理のためののコールバック関数です・
 マウス操作の処理と描画処理を行います。
 
 
 
その他：
　処理速度が若干遅いです。
　バグが数個残っています。本当にすみませんm(_ _)m
　１．入れ替えたい断片画像を２つクリックしたあと、エスケープキーを押さないと入れ替わらない。
　２．エスケープキーを２回連続で押すと勝手に終了してしまう。
　
　これらについても修正していきます。
　
