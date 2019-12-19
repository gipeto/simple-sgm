
from conans import ConanFile, CMake, tools


class SimpleSGM(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake_find_package"
    options = {"build_apps": [True, False]}
    default_options = {"build_apps": True}

    def requirements(self):
        if self.options.build_apps:
            self.requires('stb/20190512@conan/stable')

    def imports(self):
        self.copy("*.dll", dst="bin", src="bin")
        self.copy("*.dylib*", dst="lib", src="lib")
        self.copy('*.so*', dst='lib', src='lib')
