# build status

[![windows](https://github.com/nnkgw/ice-simplifier/workflows/windows/badge.svg)](https://github.com/nnkgw/ice-simplifier/actions?query=workflow%3Awindows)
[![macos](https://github.com/nnkgw/ice-simplifier/workflows/macos/badge.svg)](https://github.com/nnkgw/ice-simplifier/actions?query=workflow%3Amacos)
[![ubuntu](https://github.com/nnkgw/ice-simplifier/workflows/ubuntu/badge.svg)](https://github.com/nnkgw/ice-simplifier/actions?query=workflow%3Aubuntu)

# ice-simplifier

`.obj` ファイルを読み込んで intrinsic simplification で粗視化し、coarse メッシュ（`.obj`）、coarse/fine 頂点対応ファイル（`.cfmap`）、測地距離ファイル（`.ccgeo` / `.cfgeo`）を書き出すコマンドラインアプリケーション。

[intrinsic-simplification](https://github.com/HTDerekLiu/intrinsic-simplification) のコアライブラリを使用。可視化ライブラリ（polyscope）を含まない最小構成。

---

## Download

- [ice-simplifier.exe (windows)](https://raw.githubusercontent.com/nnkgw/ice-simplifier/main/bin/ice-simplifier.exe)

---

## ビルド

```bash
cd ice-simplifier
mkdir build && cd build
cmake ..
make -j8
```

libigl は `../externals/libigl` をローカルキャッシュとして使用する。存在しない場合は FetchContent で自動取得される。

---

## 使い方

```bash
./ice-simplifier <input.obj> <output.obj> [n_coarse_vertices]
```

| 引数 | 必須 | 説明 |
|------|------|------|
| `input.obj` | 必須 | 入力メッシュ（三角形メッシュ） |
| `output.obj` | 必須 | 出力先パス |
| `n_coarse_vertices` | 省略可 | 残す頂点数（デフォルト: `500`） |

### 実行例

```bash
# spot.obj を 200 頂点まで粗視化
./ice-simplifier ../../meshes/spot.obj spot_coarse.obj 200

# デフォルト（500頂点）
./ice-simplifier ../../meshes/spot.obj spot_coarse.obj
```

### 出力例

```
Loaded: 2930 vertices, 5856 faces
Coarsening: removing 2730 vertices...
Coarsened: 200 vertices, 396 faces
Written: spot_coarse.obj
Written: spot_coarse.cfmap
Written: spot_coarse.ccgeo
Written: spot_coarse.cfgeo
```

`output.obj` と同名の `.cfmap` / `.ccgeo` / `.cfgeo` ファイルが常に生成される。

---

## ファイル構成

```
ice-simplifier/
├── CMakeLists.txt        # ビルド設定
├── cmake/
│   └── libigl.cmake      # libigl 取得スクリプト
├── main.cpp              # アプリケーション本体
├── write_cfmap.h         # .cfmap 書き出し関数
├── write_geodesics.h     # .ccgeo / .cfgeo 書き出し関数
└── README.md             # このファイル
```

---

## 処理の流れ

```
input.obj
    │
    ▼  igl::read_triangle_mesh()
    VO: 頂点座標 (|V|×3)
    FO: 面リスト (|F|×3)
    │
    ▼  build_intrinsic_info()
    G:    グルーマップ    (|F|×6)
    l:    辺長さ          (|F|×3)
    A:    角度座標        (|F|×3)
    v2fs: 頂点→face-side (|V|×2)
    │
    ▼  coarsen_mesh()          ← F, G, l, A, v2fs をインプレース更新
    BC:  fine頂点の重心座標 (|V_fine|×3, coarse頂点分は∞で初期化)
    F2V: coarse面 → fine頂点インデックスリスト
    │
    ▼  remove_unreferenced_intrinsic()  ← F2V も含む完全版 overload
    IMV:  fine頂点インデックス → coarse頂点インデックス（生存分のみ）
    vIdx: coarse頂点インデックス（元V空間）
    F, G, l, A, v2fs, F2V を新インデックスに更新
    │
    ▼  igl::slice(VO, vIdx) + F
    V_coarse: coarse 頂点座標
    F_coarse: coarse 面リスト（新インデックス）
    │
    ├─▶  igl::write_triangle_mesh()  →  output.obj
    │
    ├─▶  write_cfmap()               →  output.cfmap
    │
    └─▶  write_geodesics()           →  output.ccgeo
                                        output.cfgeo
```

---

## .cfmap フォーマット（Coarse-Fine Map）

`output.obj` と同名で `.cfmap` ファイルが生成される。fine メッシュの各頂点が coarse メッシュのどの頂点・面に対応するかを記述する。

### 構造

```
<n_fine> <n_coarse> <n_coarse_faces>
<nearest_coarse_v> <coarse_face> <bc0> <bc1> <bc2>
<nearest_coarse_v> <coarse_face> <bc0> <bc1> <bc2>
...                                         （n_fine 行）
```

### 列の意味

| 列 | 型 | 意味 |
|----|----|------|
| `nearest_coarse_v` | int | この fine 頂点の最近傍 coarse 頂点インデックス（LRA アンカー） |
| `coarse_face` | int | BC が属する coarse 面インデックス。`-1` = この fine 頂点自身が coarse 頂点 |
| `bc0 bc1 bc2` | float | barycentric 座標（和 ≈ 1.0） |

### fine 頂点の位置復元

```
coarse_face == -1:
    p = V_coarse[nearest_coarse_v]

coarse_face >= 0:
    p = bc0·V_coarse[F[coarse_face, 0]]
      + bc1·V_coarse[F[coarse_face, 1]]
      + bc2·V_coarse[F[coarse_face, 2]]
```

### 読み込み例（C++）

```cpp
struct FineVertCorr { int nearest_coarse, coarse_face; float bc[3]; };

std::vector<FineVertCorr> load_cfmap(const std::string& path) {
    FILE* fp = fopen(path.c_str(), "r");
    int n_fine, n_coarse, n_cf;
    fscanf(fp, "%d %d %d", &n_fine, &n_coarse, &n_cf);
    std::vector<FineVertCorr> corr(n_fine);
    for (int v = 0; v < n_fine; v++)
        fscanf(fp, "%d %d %f %f %f",
               &corr[v].nearest_coarse, &corr[v].coarse_face,
               &corr[v].bc[0], &corr[v].bc[1], &corr[v].bc[2]);
    fclose(fp);
    return corr;
}
```

---

## 測地距離ファイルフォーマット（.ccgeo / .cfgeo）

fine メッシュの辺グラフ（ユークリッド辺長をエッジ重みとした Dijkstra）で事前計算した測地距離をバイナリで書き出す。

### 共通フォーマット

```
[nrows : int32][ncols : int32]
[row0_col0 : float32][row0_col1 : float32] ... （nrows × ncols 個、行優先）
```

要素 `(r, c)` へのランダムアクセス：バイトオフセット = `8 + (r * ncols + c) * 4`

### .ccgeo（Coarse-Coarse Geodesic）

| 項目 | 値 |
|------|----|
| nrows | n_coarse |
| ncols | n_coarse |
| `[ci][cj]` | coarse 頂点 ci から coarse 頂点 cj への測地距離 |
| ファイルサイズ | 8 + n_coarse² × 4 バイト |

### .cfgeo（Coarse-Fine Geodesic）

| 項目 | 値 |
|------|----|
| nrows | n_coarse |
| ncols | n_fine |
| `[ci][fi]` | coarse 頂点 ci から fine 頂点 fi への測地距離 |
| ファイルサイズ | 8 + n_coarse × n_fine × 4 バイト |

### 読み込み例（C++）

```cpp
// .ccgeo または .cfgeo を読み込んで (r, c) 要素をランダムアクセス
float read_geodesic(FILE* fp, int ncols, int r, int c) {
    float v;
    fseek(fp, 8 + (r * ncols + c) * sizeof(float), SEEK_SET);
    fread(&v, sizeof(float), 1, fp);
    return v;
}

// LRA 生成例：アタッチメント coarse 頂点 ac の行を全て読む
std::vector<float> load_cfgeo_row(const std::string& path, int ac) {
    FILE* fp = fopen(path.c_str(), "rb");
    int n_coarse, n_fine;
    fread(&n_coarse, sizeof(int), 1, fp);
    fread(&n_fine,   sizeof(int), 1, fp);
    std::vector<float> row(n_fine);
    fseek(fp, 8 + ac * n_fine * sizeof(float), SEEK_SET);
    fread(row.data(), sizeof(float), n_fine, fp);
    fclose(fp);
    return row;  // row[fi] = dist(ac, fi)
}
```

---

## アルゴリズム概要

SIGGRAPH 2023 論文 **"Surface Simplification using Intrinsic Error Metrics"** (Liu et al.) の実装。

頂点を貪欲に削除する。各頂点の削除コストは **CETM（Conformal Equivalence of Triangle Meshes）** による辺長スケーリングで求めた最適輸送コスト。`mixture_weight` パラメータ（コード内 `weight`）で誤差の種類を制御する。

| `weight` | 誤差の種類 |
|----------|-----------|
| `0.0` | 曲率誤差（デフォルト・推奨） |
| `1.0` | 面積誤差 |
| `0.0〜1.0` | 混合 |

### 頂点種別ごとの削除手順

| 種別 | 手順 |
|------|------|
| 内部頂点 | 次数3になるまでエッジをflip → 次数3頂点を削除 |
| Ear頂点 | そのまま削除 |
| 境界頂点 | Ear頂点になるまでエッジをflip → 削除 |

削除後は局所的に Delaunay 条件を回復する。

---

## 注意事項

- 入力メッシュは**1連結成分の多様体三角形メッシュ**である必要がある
- 出力の辺長は intrinsic 辺長とは一致しない（可視化用途では問題なし）
- `n_coarse_vertices` が入力頂点数以上の場合は入力をそのまま出力する

---

## 依存ライブラリ

| ライブラリ | 用途 |
|-----------|------|
| [libigl](https://libigl.github.io/) v2.4.0 | メッシュ I/O、スライス |
| [Eigen](https://eigen.tuxfamily.org/) | 線形代数 |
