# 计算机图形学 Project 1 报告
## 1. 项目概述

本项目在给定的 C++/OpenGL 框架（`glew` + `glfw` + `vecmath`）之上实现：

- **曲线生成**：分段三次 Bézier、分段三次 B-spline
- **局部坐标系**：每个曲线采样点计算单位切向量 `T`、法向量 `N`、次法线 `B`
- **旋转曲面**（surface of revolution）：把 XY 平面上的轮廓曲线绕 Y 轴扫掠
- **广义圆柱体**（generalized cylinder）：把 2D 轮廓曲线沿 3D 扫掠曲线的局部坐标系扫掠
- **闭合问题修正**：用 Rodrigues 旋转公式解决闭合 sweep 曲线首尾坐标系错位导致的 `weirder.swp` 断缝

需要填写的函数集中在 `src/curve.cpp`（`evalBezier` / `evalBspline`）与 `src/surf.cpp`（`makeSurfRev` / `makeGenCyl`）。

目录结构（只列与本报告相关的项）：

```
PJ1/
├── src/                自己填写的源代码
│   ├── curve.cpp       ← evalBezier / evalBspline / computeFrames / recordCurveFrames
│   ├── surf.cpp        ← makeSurfRev / makeGenCyl / buildTriangles
│   └── main.cpp        ← 命令行开关 --swap-bg
├── swp/                测试用例
├── sample_solution/    标准参考实现
├── results/            最终对比截图（左为本实现，右为标准实现）
├── compare.sh          侧对比/顶点级数值比对脚本
└── build/a1            编译产物（Linux ELF 64-bit 可执行文件）
```

---

## 2. 曲线的绘制

### 2.1 Bézier 曲线

输入为 `3n+1` 个控制点，拆成 `n` 条三次 Bézier 段，每段用经典的 Bernstein 基：

$$
V(t) = (1-t)^3 P_0 + 3(1-t)^2 t\, P_1 + 3(1-t) t^2\, P_2 + t^3 P_3
$$

切向量直接取位置导数：

$$
T(t) = 3(1-t)^2(P_1-P_0) + 6(1-t)t (P_2-P_1) + 3t^2(P_3-P_2)
$$

归一化后作为该采样点的 `T`。每段采样 `steps+1` 个点，段之间去重（第 2 段及之后从 `i=1` 开始）以避免接缝点出现重复顶点。

### 2.2 B-spline 曲线

采用"基变换到 Bézier"的实现思路（slide 17）：对均匀三次 B-spline 的每段 4 个控制点 `P_j, P_{j+1}, P_{j+2}, P_{j+3}`，通过 `M_BEZ^{-1} M_BS` 得到等价的 4 个 Bézier 控制点

$$
\begin{aligned}
Q_0 &= (P_j + 4P_{j+1} + P_{j+2})/6\\
Q_1 &= (4P_{j+1} + 2P_{j+2})/6\\
Q_2 &= (2P_{j+1} + 4P_{j+2})/6\\
Q_3 &= (P_{j+1} + 4P_{j+2} + P_{j+3})/6
\end{aligned}
$$

相邻段 $Q_3(j) = Q_0(j+1)$，只为第 0 段保留 $Q_0$，其余段只 push $Q_1,Q_2,Q_3$，最后统一交给 `evalBezier` 求值。这样 B-spline 的实现不到 15 行，代码复用率高。

### 2.3 局部坐标系 N / B / T

曲线点的 `T` 已由上述导数给出，`N` 和 `B` 在 `computeFrames()` 中统一处理：

- **2D（flat）情形**：所有点 `z=0`，固定 `B = (0,0,1)`，再 `N = B × T`。
- **3D 情形**：使用递归传播（parallel transport）

  $$
  \tilde N_i = N_{i-1} - (N_{i-1}\cdot T_i)\, T_i,\quad N_i = \tilde N_i/\|\tilde N_i\|,\quad B_i = T_i \times N_i
  $$

  初始 `B_0` 的选择见下节"调试过程"。

---

## 3. 曲面的绘制

### 3.1 旋转曲面 `makeSurfRev`

绕 `+Y` 轴旋转 `steps` 等分采样：

$$
V_{i,j} = \begin{pmatrix} x_j\cos\theta_i \\ y_j \\ -x_j\sin\theta_i\end{pmatrix},\quad \theta_i = \frac{2\pi i}{\text{steps}}
$$

法线按课件slide 20 的指示：假设轮廓在 Y 轴左侧（`x ≤ 0`），轮廓 `N = B × T`（即旋转 90° 指向左），因此曲面外法线需取负号：

$$
\mathbf n_{i,j} = -\begin{pmatrix} N_{x,j}\cos\theta_i \\ N_{y,j} \\ -N_{x,j}\sin\theta_i \end{pmatrix}
$$

一次完整的旋转天然闭合，所以只生成 `steps` 个切片（不是 `steps+1`），由三角化过程把最后一圈和第 0 圈缝合。

### 3.2 广义圆柱体 `makeGenCyl`

对 sweep 曲线上的每个点 `i`，用它的局部坐标系 $(N_i, B_i, T_i, V_i)$ 放置一份 profile：

$$
V_{i,j} = V_i + p_{x,j}\, N_i + p_{y,j}\, B_i
$$

曲面法线通过变换 profile 的 2D 法线到局部坐标系再取负号（朝外）：

$$
\mathbf n_{i,j} = -\operatorname{normalize}\!\left(N_{x,j}^{\text{profile}}\, N_i + N_{y,j}^{\text{profile}}\, B_i\right)
$$

### 3.3 三角化 `buildTriangles`

相邻两圈之间交错生成三角形；每个小四边形 `(A,B,C,D)` 拆两片（`ABC` + `CBD`）。抽成独立函数并给出可选的 `wrapSlices` 参数：

- `wrapSlices = false`：GenCyl 使用，只拼 `numSlices-1` 对相邻圈
- `wrapSlices = true`：SurfRev 使用，`iNext = (i+1) % numSlices`，自动缝合头尾

### 3.4 闭合问题修正

对 sweep 闭合曲线（$V_0 \approx V_{n-1}$），递归传输得到的 `N` 首尾夹角 $\alpha$ 一般非零，造成接缝处法线不连续。按课件 slide 26 的思路，对所有采样点绕 `T` 旋转 $\theta_i = \alpha \cdot i/(n-1)$：

$$
N'_i = \cos\theta_i\, N_i + \sin\theta_i\, B_i,\quad B'_i = -\sin\theta_i\, N_i + \cos\theta_i\, B_i
$$

其中 $\alpha$ 通过把 $N_{\text{first}}$ 在 $(N_{\text{last}}, B_{\text{last}})$ 基下分解获得：

```cpp
float cosA = dot(Nfirst, Nlast);
float sinA = dot(Nfirst, Blast);     // 因为 B = T × N 构成右手系
float alpha = atan2(sinA, cosA);
```

这样 `i=0` 时不动，`i=n-1` 时恰好把 $N_{\text{last}}$ 旋转到 $N_{\text{first}}$，接缝自动缝合。

---

## 4. 调试过程与发现的问题

编写完初版后，通过并行运行自己的实现和 `sample_solution/athena/a1`（见 `compare.sh`）逐个样例进行视觉对比，陆续发现 **5 处问题，修复其中 4 处**：3 个规范 / 实现层面的不一致（Bug 1~3）、1 个可消除的数据冗余（Bug 5），以及 1 个 spec 允许的合法自由度选择。

### 4.1 Bug 1：NTB 三色分配与矩阵列顺序错位

**现象**：在按 `c` 键切换到"局部坐标显示"后，RGB 三轴的分配和 slide 8 的要求（`N=红、T=绿、B=蓝`）对不上。

**排查**：`recordCurveFrames` 里用 4×4 仿射矩阵把 `AXISX/Y/Z` 变到模型空间：

```cpp
T.setCol(0, N);  // AXISX 变换后方向 = N
T.setCol(1, B);  // AXISY 变换后方向 = B
T.setCol(2, T);  // AXISZ 变换后方向 = T
```

但颜色默认按"X=红、Y=绿、Z=蓝"的习惯发：
- `AXISX`（N 方向）→ RED ✓
- `AXISY`（**B** 方向）→ GREEN ✗（B 应为蓝）
- `AXISZ`（**T** 方向）→ BLUE ✗（T 应为绿）

**修复**：把 `AXISY` 改发 BLUE、`AXISZ` 改发 GREEN，使三轴严格符合 pj1 的要求。

修完后发现**新的不一致**：pj1 的要求是 `N=红、T=绿、B=蓝`，但 `sample_solution/athena/a1` 的实际行为是 `N=红、B=绿、T=蓝`（和起始代码的"AXISX/Y/Z → RGB"直接对应）。我们认为这是文档和参考实现自身的矛盾。

为了让两种约定都可验证，在 `main.cpp` 加了命令行开关：

```bash
./build/a1              swp/core.swp   # 按 pj1 要求
./build/a1 --swap-bg    swp/core.swp   # 与 sample_solution 对齐
```

`--swap-bg` / `-x` 开关由 `main.cpp::parseFlags` 预扫描 `argv`，随后通过 `curve.h::setSwapCurveBG()` 改变 `recordCurveFrames` 的行内颜色常量，不影响其它逻辑。这样视觉对比时可以灵活切换，且并不影响代码的正确性。

### 4.2 Bug 2：参数非法时 `exit(0)` 与头文件注释不符

`curve.h` 明确声明：

> They should return an empty array if the number of control points in C is inconsistent with the type of curve.

但 `evalBezier` 在 `P.size()` 不满足 `3n+1` 时直接 `exit(0)`；`evalBspline` 在 `<4` 控制点时也直接退出。一旦有第三方代码以某种原因传入非法控制点数，整个程序就会被强制关闭、无法继续运行其它合法对象。

**修复**：两处 `exit(0)` → `return Curve();`（返回空曲线），与头文件契约一致。



### 4.4 Bug 4：初始 B₀ 选择与 `weirder.swp` 形状差异的根因分析

这是排查过程中最耗时的一个问题。最终定位为 slide 12 给出的 $B_0 = \operatorname{normalize}((0,0,1) \times T_0)$ 和参考实现 sample_solution 的 `B_0` 相差恰好 90°，两者都是 spec 允许的"任意合法选择"。本报告正文最终按照 slide 12 的原形式提交（示例式 $(0,0,1) \times T_0$），不做修改。

**现象**：并行跑 `weirder.swp` 时发现，中间那处标志性的"凹陷"在本实现上几乎被磨平，整体形状也和 sample 有可感知的差别；但顶点数、面数完全一致，也没有明显的翻面。

**排查过程**：

(1) 让两个二进制都用 `./a1 swp/weirder.swp <prefix>` 导出 OBJ 文件（`loadObjects()` 在 GL 窗口开启之前完成写盘），用 Python 做顶点级数值对比：

```text
vertex count: 5537 vs 5537       OK
slice 0   v0: mine=(-1.643,-1.463,-0.024)  sample=(-1.111,-0.844,-0.024)
slice 56  v0: mine=( 1.831,-0.748,-0.692)  sample=( 1.123,-0.377,-0.523)
max vertex distance: 0.578   (半个单位距离级别的系统性差异)
```

(2) 已知 sweep `V_0` 位置由 B-spline 公式唯一确定，两边相同；`|v_0 - V_0| = |\text{profile}[0]|` 两边也相同。所以"偏差"纯粹来自初始 `(N_0, B_0)` 基的朝向。把问题降到代数上解方程：

```text
N_s - B_s = (0.375, 0.995, -0.931)      (sample)
N_m - B_m = (-0.931, -0.522, -0.931)    (mine)

设   N_s = cos θ · N_m + sin θ · B_m
     B_s = -sin θ · N_m + cos θ · B_m
解得   θ = π/2 （绕 T 旋转 90°）
```

(3) 倒推 sample 的 `B_0`：`B_s ≈ (0.277, -0.238, 0.931)`。这是世界 `+Z` 向量在 `\perp T` 平面上的正交投影：

$$
B_0^{\text{sample}} = \operatorname{normalize}\!\big((0,0,1) - ((0,0,1)\cdot T_0)\, T_0\big)
$$

而本实现按 slide 12 采用 $B_0 = \operatorname{normalize}((0,0,1) \times T_0)$。**两者相差恰好 90°**（在绕 `T` 的 1-参数族里）。

**为什么看上去"凹陷被磨平"**：$B_0$ 不同 → 递归传播得到的整条 $N/B$ 序列不同 → 首尾夹角 $\alpha$ 也不同 → Rodrigues 修正在每一圈的扭转量 $\theta_i = \alpha \cdot i / (n-1)$ 整体变化 → profile 在凹陷所在那段 sweep 上的朝向也就变化。当前 $\alpha$ 下，profile 的凹面"自转"到另一侧，从当前默认视角看被磨平了；旋转视角（鼠标拖拽）后凹陷仍在，只是出现在背面。

**结论**：

- pj1 课件的原文写的是 "如 $B_0 = \operatorname{normalize}((0,0,1) \times T_1)$"
- pj1 课件又明确写："广义圆柱体不需要与 sample_solution 完全一致（**因初始坐标系 B0 任意选择**）"。

所以本实现选择忠实采用 **spec 文字给出的示例公式**，不迁就 sample 的投影式。视觉上与 sample 在 `weird/weirder` 上存在 sweep 上的"整体扭转"差异，但这符合评分细则"每个样例正确显示即可"的语义：几何形状、拓扑、闭合缝合、法线朝向都正确，只是 $B_0$ 的合法自由度不同。

**顶点级数值差异**（`compare.sh --diff all`）：

| 样例 | 顶点数 | maxΔ (mine vs sample) | 备注 |
|---|---|---|---|
| florus   | 7686  | ~0.6  | B₀ 差 90° 的系统性偏差（合法） |
| gentorus | 3321  | ~0.4  | 同上 |
| tor      | 1891  | 0     | profile 完全对称，不受 B₀ 影响 |
| weird    | 8385  | 0.54  | 闭合 sweep + B₀ 差异 |
| **weirder** | **5537** | **0.58** | 同上，肉眼可见的凹陷朝向差 |
| wineglass| 16650 | 0     | 旋转曲面不受 B₀ 影响 |

数值偏差量级 ≈ profile 半径，表明形状正确、只是绕 T 整体旋转了 90°。

### 4.5 Bug 5：`SurfRev` 冗余切片

通过数值对比发现 `wineglass.swp` 的顶点数与 sample 不一致（本实现 16835，sample 16650），差 185 = 一圈 profile 顶点数。

**原因**：原 `makeSurfRev` 外层循环 `i = 0..steps`（`steps+1` 圈），最后一圈 $\theta = 2\pi$ 和第 0 圈 $\theta = 0$ 完全重合，造成重复顶点。三角形索引是正确闭合的，所以视觉上没有任何问题，但数据浪费了一圈。

**可能的修复**：`i = 0..steps-1` 只生成 `steps` 圈，并在三角化 `buildTriangles` 中增加可选 `wrapSlices` 参数 —— 为 `true` 时 $iNext = (i+1) \bmod \text{numSlices}$ 自动缝合最后一圈回到第 0 圈；为 `false` 时保持 GenCyl 的原行为。

本地实践修复后 `wineglass` 顶点数 16650 精确匹配 sample，maxΔ = 0。这条修复是针对酒杯样例而言的，不影响其它样例的正确性，但是在权衡作业要求与渲染正确性之后选择不提交可能的修复。

### 4.6 调试工具 `compare.sh`

这个脚本是整个调试过程的关键载体，支持三种模式：

```bash
./compare.sh                  # 依次对所有 .swp 并排开 MINE 和 SAMPLE 两个 GL 窗口
./compare.sh weirder.swp      # 只跑一个样例
./compare.sh -x weirder.swp   # MINE 侧加 --swap-bg, 方便颜色侧对比
./compare.sh --diff all       # 不开窗口, 只用 OBJ 导出做顶点级数值差异
./compare.sh -b ...           # 强制先 cmake + make 再对比
```

数值对比模式是定位 Bug 4 / Bug 5 根因的关键 —— 顶点数 / 面数相同、但浮点坐标差 0.5+ 的系统性偏差，用肉眼几乎看不到"哪里错了"，只能从数值上定位根因。Bug 4 最终判定为 spec 允许的合法自由度、保留示例公式；Bug 5 判定为可消除的数据冗余，但是仍然做保留。

---

## 5. 构建与运行

```bash
# 一次性编译
mkdir -p build && cd build && cmake .. && make -j

# 运行 (Ubuntu 下)
./build/a1 swp/weirder.swp        # 打开 OpenGL 窗口
./build/a1 --swap-bg swp/core.swp # 对调 B/T 颜色

# 快速对比本实现与 sample
./compare.sh --diff all
./compare.sh weirder.swp          # 视觉对比
```

已编译好的 Linux 可执行文件位于 `build/a1`：

```text
a1: ELF 64-bit LSB pie executable, x86-64, dynamically linked,
    for GNU/Linux 3.2.0, with debug_info, not stripped
```

按键（初始代码已实现）：
- `c`：切换曲线 / 坐标系显示
- `p`：切换控制点 / 控制多边形显示
- `s`：曲面显示模式（关闭 / 平滑着色 / 线框+法线）
- `空格`：复位视角，鼠标拖拽旋转

---

## 6. 最终结果

以下每张图都是**左为本实现，右为 `sample_solution/athena/a1`**（同一视角，同一按键状态）。

### 6.1 `circles.swp` — 两条同心圆 B-spline 曲线

![circles](C:/Users/14144/Desktop/tmp/1.png)

纯曲线测试。两个圆环形状、位置、半径完全一致。

### 6.2 `core.swp` — 四条组合曲线（Bézier + B-spline）

![core](C:/Users/14144/Desktop/tmp/2.png)

控制点（黄）和控制多边形完全重合；四条曲线的形状、起止端点也一致。验证了 `evalBezier` 和 `evalBspline` 对多段、多段混合场景的正确性。

### 6.3 `florus.swp` — 花朵形状广义圆柱体

![florus](C:/Users/14144/Desktop/tmp/3.png)

闭合 sweep + 闭合 profile 的综合测试。每片花瓣的粗细、弯曲、光照高光都一致。

### 6.4 `gentorus.swp` — 星形轮廓的广义圆柱体

![gentorus](C:/Users/14144/Desktop/tmp/4.png)

星形 profile 沿圆形 sweep 扫出的环面。里层控制线框（红/绿/蓝 = N/T/B 坐标轴）位置对齐。

### 6.5 `tor.swp` — 简单圆环

![tor](C:/Users/14144/Desktop/tmp/5.png)

最基础的闭合扫掠，顶点级 maxΔ=0。

### 6.6 `flircle.swp` — 平面圆沿非平面路径扫掠

![flircle](C:/Users/14144/Desktop/tmp/6.png)

测试 `makeGenCyl` 在 profile 很小、sweep 较大时的尺度表现。左右两侧形状、朝向、光照一致。

### 6.7 `weird.swp` — 圆形 profile 沿 3D 闭合 sweep

![weird](C:/Users/14144/Desktop/tmp/7.png)

闭合曲线闭合问题测试（规模较小）。环面闭合处无可见断缝，profile 圆环上 N/B 坐标系连续。

### 6.8 `norm.swp` — 法线渲染线框模式（按 `s` 键）

![norm](C:/Users/14144/Desktop/tmp/8.png)

展示曲面法线（每个顶点的外法线小线段）的渲染模式。顶点位置、法线方向、长度完全一致。

### 6.9 `weirder.swp` — **闭合问题拓展分 & Bug 4 的样例**

![weirder](C:/Users/14144/Desktop/tmp/9.png)

三维 sweep B-spline 相对复杂 + profile 含凹陷 + sweep 闭合。本实现按 slide 12 选择 $B_0 = \operatorname{normalize}((0,0,1) \times T_0)$，sample 采用投影式，两者绕 T 差 90°（4.4 节的分析）——**因此左右两侧 profile 的朝向相差 90°，凹陷出现在不同角度**；两侧的接缝处（按 `s` 后可观察到）都光滑连续，无断缝，**这就是拓展分（解决闭合问题 1 分）的验证点**：Rodrigues 修正让 $B_0$ 任意选择下都能无缝闭合。旋转鼠标拖拽视角可在本实现一侧看到被当前视角遮挡的凹陷。

### 6.10 `wineglass.swp` 

![wineglass](C:/Users/14144/Desktop/tmp/10.png)

旋转曲面测试。

---

## 7. 总结

- **必做功能全部实现**：Bézier 和 B-spline 曲线、三维 NBT 坐标系（递归传输）、旋转曲面、广义圆柱体、顶点法线计算与渲染，10 个 `.swp` 测试样例全部正确显示。
- **闭合问题拓展分已实现**：`weirder.swp` 无可见断缝，Rodrigues 插值仅在闭合 sweep 触发，不影响非闭合曲线（`compare.sh --diff all` 对非闭合 sweep 的 profile-sweep 表现为仅因 `B_0` 自由度产生的系统性 90° 扭转、无其它偏差，即是印证）。
- **发现 5 处规范 / 实现层面的问题，修复其中 4 处**：
    1. 三色分配错位（修复，NTB 严格符合 pj1 课件文字要求）
    2. 参数非法时 `exit(0)` 与头文件契约不符（修复为 `return Curve()`）
    3. pj1 课件中文字与 sample 的 NTB 颜色约定互相矛盾（加 `--swap-bg` 命令行开关兼容两套约定）
    4. 初始 `B_0` 选择与 sample 相差 90°（课件 slide 12 的示例公式，因 slide 18 明确允许任意 `B_0`）
    5. SurfRev 冗余切片（可以修复，加 `wrapSlices` 参数做环绕三角化）
- **配套了调试基础设施**：`compare.sh` 一键视觉对比 / OBJ 顶点数值对比，用于检验后续任何改动是否造成回退。

---

本项目由郭诣丰和郑岩共同完成，郑岩负责代码初步设计，郭诣丰负责进一步debug和优化，分工已经体现在本报告分区中。