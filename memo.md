# Coarse / Fine 対応関係のデータ構造

## 既存データの意味

| 変数 | 意味 |
|------|------|
| `vIdx` | `coarse_v → fine_v` の直接マップ（coarse頂点はfine頂点の部分集合） |
| `BC` | fine頂点 → coarse面上の重心座標 (barycentric coords) |
| `F2V[f]` | coarse面 `f` に「乗っている」fine頂点インデックスの一覧 |

`coarsen_mesh` の出力としてすでにこれらが得られる。

## お勧めのデータ構造

fine→coarse 方向で1本化するのが汎用性が高い。

```cpp
struct VertexMap {
    // fine頂点インデックス → 対応するcoarse面インデックス
    std::vector<int> fine_to_coarse_face;   // [fine_v] = coarse face index

    // fine頂点インデックス → その面上の重心座標
    Eigen::MatrixXd bc;                     // [fine_v, :] = (w0, w1, w2)

    // coarse頂点インデックス → fine頂点インデックス (vIdx そのもの)
    Eigen::VectorXi coarse_to_fine;
};
```

`F2V` を反転するだけで `fine_to_coarse_face` が作れる：

```cpp
std::vector<int> fine_to_coarse_face(n_fine_verts, -1);
for (int f = 0; f < (int)F2V.size(); f++)
    for (int v : F2V[f])
        fine_to_coarse_face[v] = f;
```

## なぜ「頂点index同士」より重心座標がよいか

coarse頂点はfine頂点の部分集合だが、**残りのfine頂点はcoarseの辺・面の内側に落ちる**。
頂点index同士の1対1対応では表現できないケースが出る。重心座標を持っておけばシグナル補間も統一的に書ける：

```cpp
// fine頂点の位置をcoarseから復元
V_fine.row(v) = bc(v,0)*V_coarse.row(F(face,0))
              + bc(v,1)*V_coarse.row(F(face,1))
              + bc(v,2)*V_coarse.row(F(face,2));
```
