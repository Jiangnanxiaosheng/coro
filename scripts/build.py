#!/usr/bin/env python3
"""
Build script for coro project
Usage: python3 scripts/build.py [options]
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

class BuildConfig:
    def __init__(self):
        self.build_type = "Release"
        self.build_dir = "build"
        self.test = False
        self.example = False
        self.networking = True
        self.clean = False
        self.generator = None
        self.target = None
        self.parallel = None  # 新增：并行构建线程数

def parse_arguments():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description="Build coro project")
    
    parser.add_argument("--build-type", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
                       default="Release", help="Build type (default: Release)")
    parser.add_argument("--build-dir", default="build", help="Build directory (default: build)")
    
    # 测试相关参数
    parser.add_argument("--test", action="store_true", help="Build tests")
    parser.add_argument("--no-test", action="store_true", help="Do not build tests")
    
    # 示例相关参数
    parser.add_argument("--example", action="store_true", help="Build examples")
    parser.add_argument("--no-example", action="store_true", help="Do not build examples")
    
    # 网络功能参数
    parser.add_argument("--networking", action="store_true", help="Enable networking features")
    parser.add_argument("--no-networking", action="store_true", help="Disable networking features")
    
    # 新增：并行构建参数
    parser.add_argument("-j", "--parallel", type=int, 
                       help="Number of parallel jobs for building (default: auto-detect)")
    
    # 其他参数
    parser.add_argument("--clean", action="store_true", help="Clean build directory before building")
    parser.add_argument("--generator", help="CMake generator to use (e.g., 'Ninja', 'Unix Makefiles')")
    parser.add_argument("--target", help="Specific target to build")
    
    return parser.parse_args()

def setup_build_config(args):
    """根据命令行参数设置构建配置"""
    config = BuildConfig()
    config.build_type = args.build_type
    config.build_dir = args.build_dir
    config.clean = args.clean
    config.parallel = args.parallel  # 设置并行线程数
    
    # 处理测试选项
    if args.test:
        config.test = True
    elif args.no_test:
        config.test = False
    else:
        config.test = False
    
    # 处理示例选项
    if args.example:
        config.example = True
    elif args.no_example:
        config.example = False
    else:
        config.example = False
    
    # 处理网络功能选项
    if args.networking:
        config.networking = True
    elif args.no_networking:
        config.networking = False
    
    config.generator = args.generator
    config.target = args.target
    
    return config

def run_command(cmd, cwd=None):
    """运行命令并打印执行信息"""
    print(f"Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, cwd=cwd, check=True)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print(f"Command failed with error: {e}")
        return False
    except FileNotFoundError:
        print(f"Command not found: {cmd[0]}")
        return False

def get_cpu_count():
    """获取CPU核心数量，用于默认并行构建"""
    try:
        import os
        return len(os.sched_getaffinity(0))  # Linux
    except AttributeError:
        try:
            return os.cpu_count() or 4  # 其他平台
        except:
            return 4  # 默认值

def main():
    """主函数"""
    args = parse_arguments()
    config = setup_build_config(args)
    
    script_dir = Path(__file__).parent
    project_root = script_dir.parent if script_dir.name == "scripts" else script_dir
    
    build_path = project_root / config.build_dir
    
    # 清理构建目录如果请求
    if config.clean and build_path.exists():
        print(f"Cleaning build directory: {build_path}")
        import shutil
        shutil.rmtree(build_path)
    
    build_path.mkdir(exist_ok=True)
    
    # 配置CMake
    cmake_cmd = [
        "cmake",
        "-S", str(project_root),
        "-B", str(build_path),
        f"-DCMAKE_BUILD_TYPE={config.build_type}",
    ]
    
    if config.generator:
        cmake_cmd.extend(["-G", config.generator])
    
    cmake_cmd.extend([f"-DBUILD_TESTS={'ON' if config.test else 'OFF'}"])
    cmake_cmd.extend([f"-DBUILD_EXAMPLES={'ON' if config.example else 'OFF'}"])
    
    if not config.networking:
        cmake_cmd.extend(["-DNETWORKING=OFF"])
    
    if not run_command(cmake_cmd):
        print("CMake configuration failed")
        sys.exit(1)
    
    # 构建命令
    build_cmd = ["cmake", "--build", str(build_path)]
    
    # 添加并行构建选项
    if config.parallel:
        build_cmd.extend(["--parallel", str(config.parallel)])
    else:
        # 默认使用CPU核心数
        cpu_count = get_cpu_count()
        build_cmd.extend(["--parallel", str(cpu_count)])
        print(f"Using parallel build with {cpu_count} jobs")
    
    # 添加其他构建选项
    if config.target:
        build_cmd.extend(["--target", config.target])
    
    # Visual Studio 多配置处理
    if config.generator and "Visual Studio" in config.generator:
        build_cmd.extend(["--config", config.build_type])
    
    if not run_command(build_cmd):
        print("Build failed")
        sys.exit(1)
    
    print(f"\nBuild completed successfully in {config.build_type} mode")
    print(f"Build directory: {build_path}")
    
    # 运行测试如果构建了测试
    if config.test and (args.test or not args.no_test):
        test_cmd = ["ctest", "--test-dir", str(build_path)]
        
        if config.generator and "Visual Studio" in config.generator:
            test_cmd.extend(["-C", config.build_type])
        
        # 测试也使用并行
        if config.parallel:
            test_cmd.extend(["-j", str(config.parallel)])
        
        print("\nRunning tests...")
        run_command(test_cmd)

if __name__ == "__main__":
    main()