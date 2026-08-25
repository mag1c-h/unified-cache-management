# ucm

## 源码目录

```
ucm.v2/
├── CMakeLists.txt              # 顶层 CMake 入口
├── pyproject.toml              # scikit-build-core + pybind11 构建配置
├── cli/                          # C++ CLI 工具源码（→ ucm-cli 二进制，独立分发）
│   └── CMakeLists.txt
├── ucm/                          # 主包源码
│   ├── __init__.py
│   ├── shared/                   # C++ 共享实现层（→ 多个 .so，被 store 使用）
│   │   ├── logger/               # → libucm_logger.so
│   │   ├── metrics/              # → libucm_metrics.so
│   │   └── vendor/               # 预编译第三方库
│   └── v2/                       # v2 主版本
│       ├── __init__.py           # 空（由 v2/*.py 按需导入 _ucm_v2）
│       ├── *.py                  # Python 用户接口
│       ├── detail/               # C++ 内部实现（编进 store）
│       ├── proxy/                # pybind11 桥接 C++（→ _ucm_v2.*.so）
│       ├── store/                # C++ 实现层（一目录一 .so，链 shared）
│       └── transfer/             # C++ 后端 kernel（编进 store）
│           ├── cuda/             # CUDA 后端
│           └── ascend/           # 昇腾后端
├── tests/  examples/  benchmarks/  docs/  docker/  scripts/
```

## wheel 包结构

`pip install ucm` 后在 site-packages 中的形态：

```
ucm/
├── __init__.py
└── v2/
    ├── __init__.py                              # 空
    ├── *.py                                      # 用户接口
    ├── _ucm_v2.cpython-3XX-arch-linux-gnu.so   # pybind 模块（私有）
    └── _libs/                                    # 原生依赖集中地（无 __init__.py）
        ├── libucm_logger.so
        ├── libucm_metrics.so
        ├── libucm_store_<sub>.so
        └── <vendor .so>
```

- C++ 源码目录不进 wheel，构建期被各层 .so 消化
- `ucm-cli` 二进制不进 wheel，独立分发
