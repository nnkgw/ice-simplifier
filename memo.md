# Coarse / Fine 対応関係のデータ構造

## coarsen_mesh + remove_unreferenced_intrinsic 後の変数

| 変数 | 型 | 意味 |
|------|-----|------|
| `BC` | `MatrixXd` (`n_fine × 3`) | fine頂点インデックスで直接引ける重心座標行列。coarse頂点（生存分）は `∞` のまま、削除済み fine 頂点の行に座標が入る |
| `F2V[f]` | `vector<vector<int>>` | coarse面 `f` に「乗っている」fine頂点インデックスのリスト。`remove_unreferenced_intrinsic`（完全版 overload）呼び出し後は新しい coarse face インデックスに更新済み |
| `IMV` | `map<int,int>` | `fine_v → coarse_v`（生存頂点のみ）。`IMV.count(v) > 0` なら v は coarse 頂点 |
| `vIdx` | `VectorXi` | `coarse_v → fine_v`（IMV の逆）。`igl::slice(VO, vIdx, 1, V_coarse)` で coarse 座標を取得 |

## 重要：remove_unreferenced_intrinsic は完全版 overload を使うこと

```cpp
// NG: F2V が更新されず face インデックスがずれる
remove_unreferenced_intrinsic(F, IMV, IMF, vIdx, fIdx);

// OK: F2V も新インデックスに更新される
remove_unreferenced_intrinsic(F, G, l, A, v2fs, F2V, IMV, IMF, vIdx, fIdx);
```

## fine_to_face の構築

`F2V` を反転して fine 頂点→coarse 面インデックスのマップを得る：

```cpp
std::vector<int> fine_to_face(n_fine, -1);
for (int f = 0; f < (int)F2V.size(); f++)
    for (int v : F2V[f])
        fine_to_face[v] = f;
// fine_to_face[v] == -1  → v は coarse 頂点（IMV に含まれる）
// fine_to_face[v] >= 0   → v は削除済み、coarse 面 f に乗っている
```

## .cfmap への書き出し（write_cfmap.h）

`write_cfmap()` が上記ロジックをまとめて `.cfmap` ファイルに書き出す。

```cpp
write_cfmap(output_path, (int)VO.rows(), V_coarse, F_coarse, BC, F2V, IMV);
```

フォーマット詳細は README.md の「.cfmap フォーマット」節を参照。

## 測地距離ファイル（write_geodesics.h）

`write_geodesics()` が `.ccgeo` と `.cfgeo` を同時に書き出す。

```cpp
write_geodesics(output_path, VO, FO, vIdx);
```

- `VO`, `FO` — coarsen_mesh() 前の元 fine メッシュ（main.cpp で保持済み）
- `vIdx` — remove_unreferenced_intrinsic() の出力（coarse 頂点の元インデックス）

### アルゴリズム

1. `build_fine_graph(VO, FO)` で辺隣接グラフを構築（ユークリッド辺長を重みに）
2. 各 coarse 頂点 `ci`（元インデックス `vIdx(ci)`）を起点に Dijkstra を実行
3. 同一 Dijkstra 結果から `.ccgeo` と `.cfgeo` の行を同時書き出し
4. Dijkstra 実行回数 = n_coarse 回（計算量 O(n_coarse × E log V)）

### バイナリフォーマット

```
[nrows : int32][ncols : int32]
float32 row-major データ (nrows × ncols 個)
```

要素 (r, c) のバイトオフセット = `8 + (r * ncols + c) * 4`

| ファイル | nrows | ncols | 意味 |
|---------|-------|-------|------|
| `.ccgeo` | n_coarse | n_coarse | coarse-coarse 全対測地距離 |
| `.cfgeo` | n_coarse | n_fine | coarse 頂点→全 fine 頂点の測地距離 |

### auto-lra での利用

```cpp
// アタッチメント fine 頂点 a の最近傍 coarse 頂点を .cfmap から取得
int ac = cfmap[a].nearest_coarse;

// .cfgeo から行 ac を読んで全 fine 頂点への距離を得る
auto dists = load_cfgeo_row("output.cfgeo", ac);

// LRA maxDist = dists[fine_v] * (1 + slackMul)
```

毎フレームの Dijkstra（known issue B2）が不要になる。

---

## fine頂点の位置復元

```cpp
if (IMV.count(v)) {
    // coarse 頂点：座標を直接使う
    p = V_coarse.row(IMV[v]);
} else {
    // 削除済み fine 頂点：重心座標で補間
    int f = fine_to_face[v];
    p = BC(v,0)*V_coarse.row(F_coarse(f,0))
      + BC(v,1)*V_coarse.row(F_coarse(f,1))
      + BC(v,2)*V_coarse.row(F_coarse(f,2));
}
```
