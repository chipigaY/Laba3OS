#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>
using namespace std;
namespace fs = filesystem;

// ==================== ЗАДАНИЕ 1 ====================

class DirectoryMonitor {
private:
    fs::path directory_path;

public:
    DirectoryMonitor(const string& path) : directory_path(path) {
        if (!fs::exists(directory_path) || !fs::is_directory(directory_path)) {
            throw runtime_error("Указанный путь не существует или не является каталогом");
        }
    }

    // Проверка, является ли файл исполняемым .sh файлом
    bool isExecutableShFile(const fs::path& file_path) {
        if (file_path.extension() != ".sh") {
            return false;
        }

        // Проверка прав на выполнение
        fs::perms p = fs::status(file_path).permissions();
        return (p & fs::perms::owner_exec) != fs::perms::none ||
            (p & fs::perms::group_exec) != fs::perms::none ||
            (p & fs::perms::others_exec) != fs::perms::none;
    }

    // Получить список исполняемых .sh файлов
    vector<fs::path> getExecutableShFiles() {
        vector<fs::path> executable_files;

        try {
            for (const auto& entry : fs::directory_iterator(directory_path)) {
                if (entry.is_regular_file() && isExecutableShFile(entry.path())) {
                    executable_files.push_back(entry.path());
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            cerr << "Ошибка доступа к каталогу: " << e.what() << endl;
        }

        return executable_files;
    }

    // Запуск файла и удаление после завершения
    void runAndDeleteFile(const fs::path& file_path) {
        pid_t pid = fork();

        if (pid == -1) {
            cerr << "Ошибка при создании процесса для файла: " << file_path << endl;
            return;
        }

        if (pid == 0) {
            // Дочерний процесс
            cout << "Запуск файла: " << file_path << endl;

            // Запускаем скрипт
            execl("/bin/sh", "sh", file_path.c_str(), nullptr);

            // Если execl вернул управление, значит произошла ошибка
            cerr << "Ошибка запуска файла: " << file_path << endl;
            exit(1);
        }
        else {
            // Родительский процесс
            int status;
            waitpid(pid, &status, 0);  // Ждем завершения дочернего процесса

            if (WIFEXITED(status)) {
                cout << "Файл " << file_path << " завершился с кодом: " << WEXITSTATUS(status) << endl;
            }

            // Удаляем файл после выполнения
            try {
                if (fs::remove(file_path)) {
                    cout << "Файл " << file_path << " удален" << endl;
                }
                else {
                    cerr << "Не удалось удалить файл: " << file_path << endl;
                }
            }
            catch (const fs::filesystem_error& e) {
                cerr << "Ошибка при удалении файла " << file_path << ": " << e.what() << endl;
            }
        }
    }

    // Основной цикл мониторинга
    void startMonitoring() {
        cout << "Запуск мониторинга каталога: " << directory_path << endl;
        cout << "Для остановки нажмите Ctrl+C" << endl;

        while (true) {
            auto files = getExecutableShFiles();

            if (files.empty()) {
                // Нет файлов для выполнения - ждем
                sleep(5);  // Снижаем нагрузку на CPU
                continue;
            }

            // Запускаем все найденные файлы
            for (const auto& file : files) {
                runAndDeleteFile(file);
            }
        }
    }
};

// ==================== ЗАДАНИЕ 2 ====================

class FileCopier {
private:
    fs::path source_dir;
    fs::path dest_dir;

public:
    FileCopier(const string& source, const string& dest)
        : source_dir(source), dest_dir(dest) {

        if (!fs::exists(source_dir) || !fs::is_directory(source_dir)) {
            throw runtime_error("Исходный каталог не существует или не является каталогом");
        }

        // Создаем целевой каталог если он не существует
        if (!fs::exists(dest_dir)) {
            fs::create_directories(dest_dir);
        }
    }

    // Копирование одного файла в отдельном процессе
    void copyFileInProcess(const fs::path& source_file) {
        pid_t pid = fork();

        if (pid == -1) {
            cerr << "Ошибка при создании процесса для файла: " << source_file << endl;
            return;
        }

        if (pid == 0) {
            // Дочерний процесс
            try {
                fs::path dest_file = dest_dir / source_file.filename();
                fs::copy_file(source_file, dest_file, fs::copy_options::overwrite_existing);
                cout << "Скопирован: " << source_file << " -> " << dest_file << " (PID: " << getpid() << ")" << endl;
                exit(0);
            }
            catch (const fs::filesystem_error& e) {
                cerr << "Ошибка копирования " << source_file << ": " << e.what() << " (PID: " << getpid() << ")" << endl;
                exit(1);
            }
        }
        // Родительский процесс продолжает работу
    }

    // Основная функция копирования
    void copyAllFiles() {
        vector<pid_t> child_pids;

        cout << "Начало копирования из " << source_dir << " в " << dest_dir << endl;

        try {
            for (const auto& entry : fs::directory_iterator(source_dir)) {
                if (entry.is_regular_file()) {
                    copyFileInProcess(entry.path());
                    child_pids.push_back(getpid()); // Сохраняем PID для отслеживания

                    // Небольшая задержка для демонстрации параллельной работы
                    usleep(100000); // 100ms
                }
            }
        }
        catch (const fs::filesystem_error& e) {
            cerr << "Ошибка доступа к исходному каталогу: " << e.what() << endl;
            return;
        }

        // Ожидаем завершения всех дочерних процессов
        cout << "Ожидание завершения всех процессов копирования..." << endl;
        int status;
        while (wait(&status) > 0) {
            // Ждем завершения всех дочерних процессов
        }

        cout << "Копирование завершено!" << endl;
    }
};

// ==================== ГЛАВНАЯ ФУНКЦИЯ ====================

void printUsage() {
    cout << "Использование:" << endl;
    cout << "  Задание 1 (мониторинг каталога): ./program monitor <путь_к_каталогу>" << endl;
    cout << "  Задание 2 (копирование файлов): ./program copy <исходный_каталог> <целевой_каталог>" << endl;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return 1;
    }

    string mode = argv[1];

    try {
        if (mode == "monitor" && argc == 3) {
            // Задание 1
            DirectoryMonitor monitor(argv[2]);
            monitor.startMonitoring();

        }
        else if (mode == "copy" && argc == 4) {
            // Задание 2
            FileCopier copier(argv[2], argv[3]);
            copier.copyAllFiles();

        }
        else {
            printUsage();
            return 1;
        }
    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return 1;
    }

    return 0;
}