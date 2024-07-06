import os


def find_files(directory, extensions):
    found_files = []
    for root, dirs, files in os.walk(directory):
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                found_files.append(os.path.join(root, file))
    return found_files


def generate_cmake_config(directory):
    cmake_config_dir = os.path.join(directory, 'cmake')
    cmake_config_path = os.path.join(directory, 'cmake', 'LibraryConfig.cmake')

    os.mkdir(cmake_config_dir)

    include_files = find_files(directory, ['.h'])
    library_files = find_files(directory, ['.so', '.a'])

    with open(cmake_config_path, 'w') as cmake_config:
        cmake_config.write("# LibraryConfig.cmake\n\n")
        cmake_config.write("# Configuration to find and use libraries\n\n")

        # Set the include directories
        cmake_config.write(f'set(LIBRARY_INCLUDE_DIRS "{os.path.join(directory, "include")}")\n\n')

        # Set the library directories
        cmake_config.write(f'set(LIBRARY_LIBRARY_DIRS "{os.path.join(directory, "armhf")}")\n\n')

        # Set the libraries
        library_names = [os.path.splitext(os.path.basename(lib))[0] for lib in library_files]
        cmake_config.write(f'set(LIBRARY_LIBRARIES {";".join(library_names)})\n\n')

        # Provide the library targets
        cmake_config.write('add_library(MyLibrary INTERFACE)\n')
        cmake_config.write(f'target_include_directories(MyLibrary INTERFACE ${{LIBRARY_INCLUDE_DIRS}})\n')
        cmake_config.write(f'target_link_libraries(MyLibrary INTERFACE ${{LIBRARY_LIBRARY_DIRS}}/{library_names[0]})\n\n')

        # Provide information to the user
        cmake_config.write(f'message(STATUS "Found libraries: ${{LIBRARY_LIBRARIES}}")\n')

    print(f'CMake configuration file generated at: {cmake_config_path}')

# 用户输入路径
directory_path = input("请输入目录路径: ")

# 生成 CMake 配置文件
generate_cmake_config(directory_path)

