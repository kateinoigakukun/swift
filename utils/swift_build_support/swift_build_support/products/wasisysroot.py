import multiprocessing
import os

from . import product
from . import cmake_product
from . import llvm
from .. import shell

class WASILibc(product.Product):
    @classmethod
    def product_source_name(cls):
        return "wasi-libc"

    @classmethod
    def is_build_script_impl_product(cls):
        return False

    @classmethod
    def is_before_build_script_impl_product(cls):
        return False

    def should_build(self, host_target):
        # wasi-libc should always be built if standard library is being built for
        # WebAssembly.
        return self.args.build_wasmstdlib

    def should_test(self, host_target):
        return False

    def should_install(self, host_target):
        return False

    def build(self, host_target):
        build_root = os.path.dirname(self.build_dir)
        llvm_build_dir = os.path.join('..', build_root, '%s-%s' % ('llvm', host_target))
        build_jobs = self.args.build_jobs or multiprocessing.cpu_count()
        wasi_sysroot_path = _wasi_sysroot_install_path(self, host_target)

        shell.call([
            'make', 'install',
            '-j', str(build_jobs),
            '-C', self.source_dir,
            'OBJDIR=' + os.path.join(self.build_dir, 'obj'),
            'SYSROOT=' + WASILibc.sysroot_path(build_root, host_target),
            'INSTALL_DIR=' + wasi_sysroot_path,
            'CC=' + os.path.join(llvm_build_dir, 'bin', 'clang'),
            'AR=' + os.path.join(llvm_build_dir, 'bin', 'llvm-ar'),
            'NM=' + os.path.join(llvm_build_dir, 'bin', 'llvm-nm'),
        ])

    @classmethod
    def get_dependencies(cls):
        return [llvm.LLVM]

    @classmethod
    def sysroot_path(cls, build_root, host_target):
        return os.path.join(build_root, '%s-%s' % (cls.product_name(), host_target), 'sysroot')

class WasmLLVMRuntimeLibs(cmake_product.CMakeProduct):
    @classmethod
    def product_source_name(cls):
        return os.path.join("llvm-project", "runtimes")

    @classmethod
    def is_build_script_impl_product(cls):
        return False

    @classmethod
    def is_before_build_script_impl_product(cls):
        return False

    def should_build(self, host_target):
        # wasi-libc should always be built if standard library is being built for
        # WebAssembly.
        return self.args.build_wasmstdlib

    def should_test(self, host_target):
        return False

    def should_install(self, host_target):
        return False

    def build(self, host_target):
        build_root = os.path.dirname(self.build_dir)
        llvm_build_dir = os.path.join('..', build_root, '%s-%s' % ('llvm', host_target))

        self.cmake_options.define('CMAKE_SYSROOT:PATH', WASILibc.sysroot_path(build_root, host_target))
        self.cmake_options.define('LLVM_ENABLE_RUNTIMES:STRING', 'libcxx;libcxxabi;compiler-rt')
        self.cmake_options.define('LIBCXX_LIBDIR_SUFFIX:STRING', '/wasm32-wasi')
        self.cmake_options.define('LIBCXXABI_LIBDIR_SUFFIX:STRING', '/wasm32-wasi')
        self.cmake_options.define('CMAKE_STAGING_PREFIX:PATH', '/')

        self.cmake_options.define('COMPILER_RT_DEFAULT_TARGET_ARCH:STRING', 'wasm32')
        self.cmake_options.define('COMPILER_RT_DEFAULT_TARGET_ONLY:BOOL', 'TRUE')
        self.cmake_options.define('COMPILER_RT_BAREMETAL_BUILD:BOOL', 'TRUE')
        self.cmake_options.define('COMPILER_RT_BUILD_XRAY:BOOL', 'FALSE')
        self.cmake_options.define('COMPILER_RT_INCLUDE_TESTS:BOOL', 'FALSE')
        self.cmake_options.define('COMPILER_RT_HAS_FPIC_FLAG:BOOL', 'FALSE')
        self.cmake_options.define('COMPILER_RT_EXCLUDE_ATOMIC_BUILTIN:BOOL', 'FALSE')
        self.cmake_options.define('COMPILER_RT_OS_DIR:STRING', 'wasi')

        self.cmake_options.define('CMAKE_C_COMPILER_WORKS:BOOL', 'TRUE')
        self.cmake_options.define('CMAKE_CXX_COMPILER_WORKS:BOOL', 'TRUE')

        self.cmake_options.define('CMAKE_SYSTEM_NAME:STRING', 'WASI')
        self.cmake_options.define('CMAKE_SYSTEM_PROCESSOR:STRING', 'wasm32')
        self.cmake_options.define('CMAKE_AR:FILEPATH', os.path.join(llvm_build_dir, 'bin', 'llvm-ar'))
        self.cmake_options.define('CMAKE_RANLIB:FILEPATH', os.path.join(llvm_build_dir, 'bin', 'llvm-ranlib'))
        self.cmake_options.define('CMAKE_C_COMPILER:FILEPATH', os.path.join(llvm_build_dir, 'bin', 'clang'))
        self.cmake_options.define('CMAKE_CXX_COMPILER:STRING', os.path.join(llvm_build_dir, 'bin', 'clang++'))
        # Explicitly disable exceptions even though it's usually implicitly disabled by
        # LIBCXX_ENABLE_EXCEPTIONS because the CMake feature check fails to detect -fno-exceptions
        # support in clang due to missing compiler-rt while configuring as mono project.
        self.cmake_options.define('CMAKE_CXX_FLAGS:STRING', '-fno-exceptions')

        self.cmake_options.define('CMAKE_C_COMPILER_TARGET:STRING', 'wasm32-wasi')
        self.cmake_options.define('CMAKE_CXX_COMPILER_TARGET:STRING', 'wasm32-wasi')

        self.cmake_options.define('CXX_SUPPORTS_CXX11:BOOL', 'TRUE')

        self.cmake_options.define('LIBCXX_ENABLE_THREADS:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_HAS_PTHREAD_API:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_HAS_EXTERNAL_THREAD_API:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_BUILD_EXTERNAL_THREAD_LIBRARY:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_HAS_WIN32_THREAD_API:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_ENABLE_SHARED:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_ENABLE_EXPERIMENTAL_LIBRARY:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_ENABLE_EXCEPTIONS:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_ENABLE_FILESYSTEM:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXX_CXX_ABI', 'libcxxabi')
        self.cmake_options.define('LIBCXX_HAS_MUSL_LIBC:BOOL', 'TRUE')

        self.cmake_options.define('LIBCXX_ABI_VERSION', '2')
        self.cmake_options.define('LIBCXXABI_ENABLE_EXCEPTIONS:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_ENABLE_SHARED:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_SILENT_TERMINATE:BOOL', 'TRUE')
        self.cmake_options.define('LIBCXXABI_ENABLE_THREADS:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_HAS_PTHREAD_API:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_HAS_EXTERNAL_THREAD_API:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_BUILD_EXTERNAL_THREAD_LIBRARY:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_HAS_WIN32_THREAD_API:BOOL', 'FALSE')
        self.cmake_options.define('LIBCXXABI_ENABLE_PIC:BOOL', 'FALSE')
        self.cmake_options.define('UNIX:BOOL', 'TRUE')

        self.build_with_cmake([], self.args.build_variant, [], prefer_just_built_toolchain=True)
        wasi_sysroot_path = _wasi_sysroot_install_path(self, host_target)
        self.install_with_cmake(["install"], wasi_sysroot_path)

    @classmethod
    def get_dependencies(cls):
        return [WASILibc, llvm.LLVM]

def _wasi_sysroot_install_path(product, host_target):
    build_root = os.path.dirname(product.build_dir)
    return os.path.join('..', build_root, 'wasi-sysroot')
