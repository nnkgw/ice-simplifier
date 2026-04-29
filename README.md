# build status

[![windows](https://github.com/nnkgw/ice-simplifier/workflows/windows/badge.svg)](https://github.com/nnkgw/ice-simplifier/actions?query=workflow%3Awindows)
[![macos](https://github.com/nnkgw/ice-simplifier/workflows/macos/badge.svg)](https://github.com/nnkgw/ice-simplifier/actions?query=workflow%3Amacos)
[![ubuntu](https://github.com/nnkgw/ice-simplifier/workflows/ubuntu/badge.svg)](https://github.com/nnkgw/ice-simplifier/actions?query=workflow%3Aubuntu)

# ice-simplifier

`.obj` ファイルを読み込んで intrinsic simplification で粗視化し、`.obj` ファイルとして書き出すコマンドラインアプリケーション。

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
```

---

## ファイル構成

```
ice-simplifier/
├── CMakeLists.txt        # ビルド設定
├── cmake/
│   └── libigl.cmake      # libigl 取得スクリプト
├── main.cpp              # アプリケーション本体
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
    BC:  削除頂点の重心座標 (n×3)
    F2V: face→BC インデックス
    │
    ▼  remove_unreferenced_intrinsic()
    vIdx: coarse 頂点インデックス（元V空間）
    │
    ▼  igl::slice(VO, vIdx) + F
    V_coarse: coarse 頂点座標
    F_coarse: coarse 面リスト
    │
    ▼  igl::write_triangle_mesh()
    output.obj
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
