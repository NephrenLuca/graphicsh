#!/usr/bin/env bash
# compare.sh - 并行运行自己的实现 (build/a1) 与 sample_solution/athena/a1，便于视觉对比
#
# 用法:
#   ./compare.sh                  # 遍历 swp/ 下所有 .swp 文件，逐个对比
#   ./compare.sh <file.swp>       # 对比指定 swp 文件 (相对 swp/ 或完整路径均可)
#   ./compare.sh all              # 等价于无参
#   ./compare.sh -b               # 先执行 build/ 编译再运行 (若未编译则自动触发)
#   ./compare.sh -x <file.swp>    # 给 MINE 传 --swap-bg (对调 B/T 颜色, 与 sample 一致)
#   ./compare.sh --diff <file>    # 仅生成两份 OBJ 并打印顶点差异 (不开窗口)
#
# 运行过程中：两个 OpenGL 窗口并排弹出（左为自己的实现，右为 sample）。
# 关闭任一窗口后脚本继续下一个样例；按 Ctrl+C 可强制跳过并结束。

set -u

PROJ_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MINE="$PROJ_ROOT/build/a1"
SAMPLE="$PROJ_ROOT/sample_solution/athena/a1"
SWP_DIR="$PROJ_ROOT/swp"

color() { printf "\033[%sm%s\033[0m" "$1" "$2"; }
info()  { echo -e "$(color 36 '[INFO]')  $*"; }
warn()  { echo -e "$(color 33 '[WARN]')  $*"; }
err()   { echo -e "$(color 31 '[ERR ]')  $*" >&2; }

do_build=0
swap_bg=0
diff_only=0
target_arg=""
for arg in "$@"; do
    case "$arg" in
        -b|--build)    do_build=1 ;;
        -x|--swap-bg)  swap_bg=1 ;;
        --diff)        diff_only=1 ;;
        -h|--help)
            sed -n '2,14p' "$0"; exit 0 ;;
        *) target_arg="$arg" ;;
    esac
done

if [[ $do_build -eq 1 || ! -x "$MINE" ]]; then
    info "编译 build/a1 ..."
    mkdir -p "$PROJ_ROOT/build"
    ( cd "$PROJ_ROOT/build" && cmake .. >/dev/null && make -j ) || { err "编译失败"; exit 1; }
fi

[[ -x "$MINE" ]]  || { err "找不到自己的实现: $MINE (请先编译)";            exit 1; }
[[ -x "$SAMPLE" ]] || { chmod a+x "$SAMPLE" 2>/dev/null || true; }
[[ -x "$SAMPLE" ]] || { err "找不到 sample_solution/athena/a1"; exit 1; }

# 收集要对比的 swp 文件
declare -a files=()
if [[ -z "$target_arg" || "$target_arg" == "all" ]]; then
    while IFS= read -r -d '' f; do files+=("$f"); done < <(find "$SWP_DIR" -maxdepth 1 -name '*.swp' -print0 | sort -z)
else
    if [[ -f "$target_arg" ]]; then
        files+=("$target_arg")
    elif [[ -f "$SWP_DIR/$target_arg" ]]; then
        files+=("$SWP_DIR/$target_arg")
    elif [[ -f "$SWP_DIR/$target_arg.swp" ]]; then
        files+=("$SWP_DIR/$target_arg.swp")
    else
        err "找不到 swp 文件: $target_arg"; exit 1
    fi
fi

info "共 ${#files[@]} 个样例待对比"
info "  MINE  : $MINE"
info "  SAMPLE: $SAMPLE"

# 尝试用 wmctrl 把两个窗口左右排列（可选依赖）
have_wmctrl=0
command -v wmctrl >/dev/null 2>&1 && have_wmctrl=1

arrange_windows() {
    [[ $have_wmctrl -eq 1 ]] || return 0
    local mine_pid=$1 sample_pid=$2
    # 等待窗口真正出现
    local tries=20
    while (( tries-- > 0 )); do
        local info
        info=$(wmctrl -lp 2>/dev/null || true)
        if echo "$info" | grep -q " $mine_pid " && echo "$info" | grep -q " $sample_pid "; then
            break
        fi
        sleep 0.25
    done
    local screen_w
    screen_w=$(xdpyinfo 2>/dev/null | awk '/dimensions:/ {print $2}' | cut -dx -f1)
    [[ -z "$screen_w" ]] && screen_w=1920
    local half=$(( screen_w / 2 ))
    while read -r win _ pid _; do
        [[ "$pid" == "$mine_pid"   ]] && wmctrl -ir "$win" -e "0,0,50,${half},700" 2>/dev/null
        [[ "$pid" == "$sample_pid" ]] && wmctrl -ir "$win" -e "0,${half},50,${half},700" 2>/dev/null
    done < <(wmctrl -lp 2>/dev/null)
}

mine_flags=()
(( swap_bg == 1 )) && mine_flags+=("--swap-bg")

diff_objs() {
    local swp="$1"
    local tmpd
    tmpd=$(mktemp -d)
    mkdir -p "$tmpd/mine" "$tmpd/sample"
    timeout 5 "$MINE"   "${mine_flags[@]}" "$swp" "$tmpd/mine/m"   >/dev/null 2>&1
    timeout 5 "$SAMPLE"                    "$swp" "$tmpd/sample/s" >/dev/null 2>&1
    python3 - "$tmpd" "$(basename "$swp")" <<'PY'
import os, sys, glob, math
tmpd, name = sys.argv[1], sys.argv[2]
mfs = sorted(glob.glob(f'{tmpd}/mine/*.obj'))
sfs = sorted(glob.glob(f'{tmpd}/sample/*.obj'))
if not mfs:   print(f'{name}: mine produced no obj (curves-only?)'); sys.exit()
if not sfs:   print(f'{name}: sample produced no obj'); sys.exit()
def lv(p): return [tuple(map(float,l.split()[1:4])) for l in open(p) if l.startswith('v ')]
for mf, sf in zip(mfs, sfs):
    m, s = lv(mf), lv(sf)
    if len(m) != len(s):
        print(f'{name} {os.path.basename(mf)}: vcount differ mine={len(m)} sample={len(s)}'); continue
    d = [math.sqrt(sum((a-b)**2 for a,b in zip(u,v))) for u,v in zip(m,s)]
    print(f'{name} {os.path.basename(mf)}: n={len(m):<6} maxΔ={max(d):.5f} meanΔ={sum(d)/len(d):.5f}')
PY
    rm -rf "$tmpd"
}

run_pair() {
    local swp="$1"
    local base
    base=$(basename "$swp")
    echo
    info "=== $(color 35 "$base") ==="

    if (( diff_only == 1 )); then
        diff_objs "$swp"
        return
    fi

    # 启动两个进程 (MINE 带 flags, SAMPLE 不带)
    "$MINE"   "${mine_flags[@]}" "$swp" > /tmp/mine_$$.log 2>&1 &
    local mine_pid=$!
    "$SAMPLE"                    "$swp" > /tmp/sample_$$.log 2>&1 &
    local sample_pid=$!

    info "  MINE  pid=$mine_pid   SAMPLE pid=$sample_pid"
    arrange_windows "$mine_pid" "$sample_pid" &

    # 等待任一退出
    while kill -0 "$mine_pid" 2>/dev/null && kill -0 "$sample_pid" 2>/dev/null; do
        sleep 0.3
    done

    # 给用户机会关闭另一个窗口继续观察；10 秒后强制结束剩余进程
    local wait_left=100
    while (( wait_left-- > 0 )) && ( kill -0 "$mine_pid" 2>/dev/null || kill -0 "$sample_pid" 2>/dev/null ); do
        sleep 0.1
    done
    kill "$mine_pid"   2>/dev/null || true
    kill "$sample_pid" 2>/dev/null || true
    wait "$mine_pid"   2>/dev/null || true
    wait "$sample_pid" 2>/dev/null || true

    # 检查 MINE 的异常退出 (非 0) 且有输出 — 可能是参数非法被拒绝等
    if grep -qiE 'error|must be called' /tmp/mine_$$.log 2>/dev/null; then
        warn "  MINE stderr:"
        sed 's/^/    /' /tmp/mine_$$.log
    fi
    rm -f /tmp/mine_$$.log /tmp/sample_$$.log
}

trap 'echo; warn "中断，清理子进程"; pkill -P $$ 2>/dev/null; exit 130' INT

for f in "${files[@]}"; do
    run_pair "$f"
done

info "全部对比完成"
