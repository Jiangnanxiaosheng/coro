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

# 定义构建配置类，用于存储所有构建参数
class BuildConfig:
    def __init__(self):
        self.build_type = "Release"  # 构建类型，默认为Release
        self.build_dir = "build"     # 构建目录，默认为build
        self.test = False           # 是否构建测试，初始为False
        self.example = False        # 是否构建示例，初始为False
        self.networking = True      # 是否启用网络功能，默认为True
        self.clean = False          # 是否清理构建目录，默认为False
        self.generator = None       # CMake生成器，如Ninja等
        self.target = None          # 特定构建目标

def parse_arguments():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description="Build coro project")
    
    # 构建类型参数，可选Debug/Release/RelWithDebInfo/MinSizeRel
    parser.add_argument("--build-type", choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
                       default="Release", help="Build type (default: Release)")
    
    # 构建目录参数
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
    
    # 其他参数
    parser.add_argument("--clean", action="store_true", help="Clean build directory before building")
    parser.add_argument("--generator", help="CMake generator to use (e.g., 'Ninja', 'Unix Makefiles')")
    parser.add_argument("--target", help="Specific target to build")
    
    return parser.parse_args()

def setup_build_config(args):
    """根据命令行参数设置构建配置"""
    config = BuildConfig()
    config.build_type = args.build_type  # 设置构建类型
    config.build_dir = args.build_dir    # 设置构建目录
    config.clean = args.clean            # 设置清理标志
    
    # 处理测试选项的逻辑
    if args.test:           # 如果指定了--test，强制开启测试
        config.test = True
    elif args.no_test:      # 如果指定了--no-test，强制关闭测试
        config.test = False
    
    # 处理示例选项的逻辑（同上）
    if args.example:
        config.example = True
    elif args.no_example:
        config.example = False

    
    # 处理网络功能选项
    if args.networking:     # 如果指定了--networking，强制开启
        config.networking = True
    elif args.no_networking: # 如果指定了--no-networking，强制关闭
        config.networking = False
    # 如果没指定任何网络参数，保持默认值True不变
    
    config.generator = args.generator  # 设置生成器
    config.target = args.target        # 设置特定目标
    
    return config

def run_command(cmd, cwd=None):
    """运行命令并打印执行信息"""
    print(f"Running: {' '.join(cmd)}")  # 打印正在执行的命令
    
    try:
        # 运行子进程命令，check=True表示如果命令失败会抛出异常
        result = subprocess.run(cmd, cwd=cwd, check=True)
        return result.returncode == 0  # 返回命令执行是否成功
    except subprocess.CalledProcessError as e:
        print(f"Command failed with error: {e}")  # 命令执行失败
        return False
    except FileNotFoundError:
        print(f"Command not found: {cmd[0]}")  # 命令不存在
        return False

def main():
    """主函数"""
    # 1. 解析命令行参数
    args = parse_arguments()
    
    # 2. 根据参数设置构建配置
    config = setup_build_config(args)
    
    # 3. 获取项目根目录
    # __file__是当前脚本文件的路径
    script_dir = Path(__file__).parent  # 脚本所在目录
    # 判断脚本是否在scripts目录下，如果是则项目根目录是父目录
    project_root = script_dir.parent if script_dir.name == "scripts" else script_dir
    
    # 4. 构建完整构建目录路径
    build_path = project_root / config.build_dir
    
    # 5. 如果需要清理，删除构建目录
    if config.clean and build_path.exists():
        print(f"Cleaning build directory: {build_path}")
        import shutil
        shutil.rmtree(build_path)  # 递归删除目录
    
    # 6. 创建构建目录（如果不存在）
    build_path.mkdir(exist_ok=True)
    
    # 7. 配置CMake命令
    cmake_cmd = [
        "cmake",                    # CMake命令
        "-S", str(project_root),    # 源代码目录
        "-B", str(build_path),      # 构建目录
        f"-DCMAKE_BUILD_TYPE={config.build_type}",  # 构建类型
    ]
    
    # 8. 如果指定了生成器，添加到命令中
    if config.generator:
        cmake_cmd.extend(["-G", config.generator])
    
    # 9. 添加构建选项到CMake命令
    cmake_cmd.extend([f"-DBUILD_TESTS={'ON' if config.test else 'OFF'}"])      # 测试选项
    cmake_cmd.extend([f"-DBUILD_EXAMPLES={'ON' if config.example else 'OFF'}"]) # 示例选项
    
    # 10. 处理网络功能选项（只在明确禁用时设置）
    if not config.networking:
        cmake_cmd.extend(["-DNETWORKING=OFF"])
    # 如果启用网络功能，不设置参数，让CMake使用默认逻辑
    
    # 11. 运行CMake配置命令
    if not run_command(cmake_cmd):
        print("CMake configuration failed")
        sys.exit(1)  # 配置失败，退出程序
    
    # 12. 构建命令
    build_cmd = ["cmake", "--build", str(build_path)]
    
    # 13. 如果指定了特定目标，添加到构建命令
    if config.target:
        build_cmd.extend(["--target", config.target])
    
    # 14. 如果是Visual Studio等多配置生成器，添加配置参数
    if config.generator and "Visual Studio" in config.generator:
        build_cmd.extend(["--config", config.build_type])
    
    # 15. 运行构建命令
    if not run_command(build_cmd):
        print("Build failed")
        sys.exit(1)
    
    # 16. 打印构建成功信息
    print(f"\nBuild completed successfully in {config.build_type} mode")
    print(f"Build directory: {build_path}")
    
    # 17. 如果构建了测试且用户没有明确禁止测试，运行测试
    if config.test and (args.test or not args.no_test):
        test_cmd = ["ctest", "--test-dir", str(build_path)]
        
        # 处理Visual Studio的测试配置
        if config.generator and "Visual Studio" in config.generator:
            test_cmd.extend(["-C", config.build_type])
        
        print("\nRunning tests...")
        run_command(test_cmd)

if __name__ == "__main__":
    main()