#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstring>
#include <algorithm>
#include <thread>
#include <chrono>
#include <cstdlib>
using namespace std;
class SimpleFileSystem {
public:
    static bool exists(const string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    }

    static bool is_directory(const string& path) {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) == 0) {
            return S_ISDIR(buffer.st_mode);
        }
        return false;
    }

    static bool is_regular_file(const string& path) {
        struct stat buffer;
        if (stat(path.c_str(), &buffer) == 0) {
            return S_ISREG(buffer.st_mode);
        }
        return false;
    }

    static bool remove(const string& path) {
        return ::remove(path.c_str()) == 0;
    }

    static vector<string> get_files_in_directory(const string& path) {
        vector<string> files;
        DIR* dir = opendir(path.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                if (entry->d_type == DT_REG) { // Regular file
                    files.push_back(entry->d_name);
                }
            }
            closedir(dir);
        }
        return files;
    }

    static string extension(const string& path) {
        size_t dot_pos = path.find_last_of('.');
        if (dot_pos != string::npos) {
            return path.substr(dot_pos);
        }
        return "";
    }
};

// Функция для получения домашней директории
string get_home_dir() {
    const char* home = getenv("HOME");
    if (home) return string(home);
    return "/home/" + string(getenv("USER"));
}

// ==================== ЗАДАНИЕ 1 ====================

class DirectoryMonitor {
private:
    string directory_path;

public:
    DirectoryMonitor(const string& path) : directory_path(path) {
        if (!SimpleFileSystem::exists(directory_path) || !SimpleFileSystem::is_directory(directory_path)) {
            throw runtime_error("Указанный путь не существует или не является каталогом");
        }
    }

    // Проверка, является ли файл исполняемым .sh файлом
    bool isExecutableShFile(const string& filename) {
        if (SimpleFileSystem::extension(filename) != ".sh") {
            return false;
        }

        string full_path = directory_path + "/" + filename;
        struct stat st;
        if (stat(full_path.c_str(), &st) == 0) {
            return (st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH);
        }
        return false;
    }

    // Получить список исполняемых .sh файлов
    vector<string> getExecutableShFiles() {
        vector<string> executable_files;
        auto all_files = SimpleFileSystem::get_files_in_directory(directory_path);

        for (const auto& file : all_files) {
            if (isExecutableShFile(file)) {
                executable_files.push_back(file);
            }
        }

        return executable_files;
    }

    // Запуск файла и удаление после завершения
    void runAndDeleteFile(const string& filename) {
        pid_t pid = fork();

        if (pid == -1) {
            cerr << "Ошибка при создании процесса для файла: " << filename << endl;
            return;
        }

        if (pid == 0) {
            // Дочерний процесс
            string full_path = directory_path + "/" + filename;
            cout << "[ЗАДАНИЕ 1] Запуск файла: " << full_path << endl;

            // Запускаем скрипт
            execl("/bin/sh", "sh", full_path.c_str(), nullptr);

            // Если execl вернул управление, значит произошла ошибка
            cerr << "Ошибка запуска файла: " << full_path << endl;
            exit(1);
        }
        else {
            // Родительский процесс
            int status;
            waitpid(pid, &status, 0);  // Ждем завершения дочернего процесса

            if (WIFEXITED(status)) {
                cout << "[ЗАДАНИЕ 1] Файл " << filename << " завершился с кодом: " << WEXITSTATUS(status) << endl;
            }

            // Удаляем файл после выполнения
            string full_path = directory_path + "/" + filename;
            if (SimpleFileSystem::remove(full_path)) {
                cout << "[ЗАДАНИЕ 1] Файл " << filename << " удален" << endl;
            }
            else {
                cerr << "[ЗАДАНИЕ 1] Не удалось удалить файл: " << filename << endl;
            }
        }
    }

    // Основной цикл мониторинга (ограниченный по времени)
    void startMonitoringLimited(int duration_seconds) {
        cout << "=== ЗАДАНИЕ 1: Запуск мониторинга каталога: " << directory_path << " на " << duration_seconds << " секунд ===" << endl;

        auto start_time = chrono::steady_clock::now();

        while (true) {
            auto current_time = chrono::steady_clock::now();
            auto elapsed = chrono::duration_cast<chrono::seconds>(current_time - start_time).count();

            if (elapsed >= duration_seconds) {
                cout << "[ЗАДАНИЕ 1] Время мониторинга истекло (" << duration_seconds << " секунд)" << endl;
                break;
            }

            auto files = getExecutableShFiles();

            if (files.empty()) {
                // Нет файлов для выполнения - ждем
                sleep(2);  // Снижаем нагрузку на CPU
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
    string source_dir;
    string dest_dir;

    static void copyFile(const string& source, const string& dest) {
        FILE* src = fopen(source.c_str(), "rb");
        FILE* dst = fopen(dest.c_str(), "wb");

        if (!src || !dst) {
            if (src) fclose(src);
            if (dst) fclose(dst);
            throw runtime_error("Ошибка открытия файлов для копирования");
        }

        char buffer[4096];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);
    }

public:
    FileCopier(const string& source, const string& dest)
        : source_dir(source), dest_dir(dest) {

        if (!SimpleFileSystem::exists(source_dir) || !SimpleFileSystem::is_directory(source_dir)) {
            throw runtime_error("Исходный каталог '" + source_dir + "' не существует или не является каталогом");
        }

        // Создаем целевой каталог если он не существует
        if (!SimpleFileSystem::exists(dest_dir)) {
            mkdir(dest_dir.c_str(), 0755);
        }
    }

    // Копирование одного файла в отдельном процессе
    void copyFileInProcess(const string& filename) {
        pid_t pid = fork();

        if (pid == -1) {
            cerr << "[ЗАДАНИЕ 2] Ошибка при создании процесса для файла: " << filename << endl;
            return;
        }

        if (pid == 0) {
            // Дочерний процесс
            try {
                string source_file = source_dir + "/" + filename;
                string dest_file = dest_dir + "/" + filename;
                copyFile(source_file, dest_file);
                cout << "[ЗАДАНИЕ 2] Скопирован: " << filename << " -> " << dest_file << " (PID: " << getpid() << ")" << endl;
                exit(0);
            }
            catch (const exception& e) {
                cerr << "[ЗАДАНИЕ 2] Ошибка копирования " << filename << ": " << e.what() << " (PID: " << getpid() << ")" << endl;
                exit(1);
            }
        }
        // Родительский процесс продолжает работу
    }

    // Основная функция копирования
    void copyAllFiles() {
        cout << "=== ЗАДАНИЕ 2: Начало копирования из " << source_dir << " в " << dest_dir << " ===" << endl;

        try {
            auto files = SimpleFileSystem::get_files_in_directory(source_dir);

            cout << "[ЗАДАНИЕ 2] Найдено файлов для копирования: " << files.size() << endl;

            for (const auto& file : files) {
                copyFileInProcess(file);

                // Небольшая задержка для демонстрации параллельной работы
                usleep(50000); // 50ms
            }
        }
        catch (const exception& e) {
            cerr << "[ЗАДАНИЕ 2] Ошибка доступа к исходному каталогу: " << e.what() << endl;
            return;
        }

        // Ожидаем завершения всех дочерних процессов
        cout << "[ЗАДАНИЕ 2] Ожидание завершения всех процессов копирования..." << endl;
        int status;
        while (wait(&status) > 0) {
            // Ждем завершения всех дочерних процессов
        }

        cout << "[ЗАДАНИЕ 2] Копирование завершено!" << endl;
    }
};

// ==================== ГЛАВНАЯ ФУНКЦИЯ ====================

int main() {
    cout << "=== ЛАБОРАТОРНАЯ РАБОТА №3 ===" << endl;
    cout << "=== СОЗДАНИЕ И УНИЧТОЖЕНИЕ ПРОЦЕССОВ В ОС Unix/Linux ===" << endl;
    cout << endl;

    // Получаем домашнюю директорию
    string home_dir = get_home_dir();
    string test_monitor_dir = home_dir + "/test_monitor";
    string source_dir = home_dir + "/source_dir";
    string dest_dir = home_dir + "/dest_dir";

    cout << "Домашняя директория: " << home_dir << endl;
    cout << "Каталог для мониторинга: " << test_monitor_dir << endl;
    cout << "Исходный каталог: " << source_dir << endl;
    cout << "Целевой каталог: " << dest_dir << endl;
    cout << endl;

    // Создаем тестовые каталоги
    cout << "СОЗДАНИЕ ТЕСТОВЫХ КАТАЛОГОВ..." << endl;
    mkdir(test_monitor_dir.c_str(), 0755);
    mkdir(source_dir.c_str(), 0755);
    mkdir(dest_dir.c_str(), 0755);

    // Подготовка тестовых данных для задания 1
    cout << "ПОДГОТОВКА ТЕСТОВЫХ ДАННЫХ..." << endl;

    // Создаем тестовые .sh скрипты
    string script1 = test_monitor_dir + "/script1.sh";
    string script2 = test_monitor_dir + "/script2.sh";

    FILE* f1 = fopen(script1.c_str(), "w");
    if (f1) {
        fprintf(f1, "#!/bin/bash\necho \"Скрипт 1 запущен в процессе $$\"\nsleep 2\necho \"Скрипт 1 завершен\"\n");
        fclose(f1);
    }

    FILE* f2 = fopen(script2.c_str(), "w");
    if (f2) {
        fprintf(f2, "#!/bin/bash\necho \"Скрипт 2 запущен в процессе $$\"\nfor i in 1 2 3; do\n    echo \"Шаг $i\"\n    sleep 1\ndone\necho \"Скрипт 2 завершен\"\n");
        fclose(f2);
    }

    // Делаем файлы исполняемыми
    chmod(script1.c_str(), 0755);
    chmod(script2.c_str(), 0755);

    // Создаем тестовые файлы для копирования
    string file1 = source_dir + "/file1.txt";
    string file2 = source_dir + "/file2.txt";
    string file3 = source_dir + "/file3.txt";

    FILE* tf1 = fopen(file1.c_str(), "w");
    if (tf1) {
        fprintf(tf1, "Содержимое файла 1 для копирования\n");
        fclose(tf1);
    }

    FILE* tf2 = fopen(file2.c_str(), "w");
    if (tf2) {
        fprintf(tf2, "Содержимое файла 2 для копирования\n");
        fclose(tf2);
    }

    FILE* tf3 = fopen(file3.c_str(), "w");
    if (tf3) {
        fprintf(tf3, "Содержимое файла 3 для копирования\n");
        fclose(tf3);
    }

    cout << "Тестовые данные подготовлены!" << endl;
    cout << endl;

    try {
        // ЗАДАНИЕ 2: Запускаем копирование файлов
        FileCopier copier(source_dir, dest_dir);
        copier.copyAllFiles();

        cout << endl;
        cout << "=== ПРОВЕРКА РЕЗУЛЬТАТОВ КОПИРОВАНИЯ ===" << endl;
        string cmd = "ls -la " + dest_dir;
        system(cmd.c_str());
        cout << endl;

        // ЗАДАНИЕ 1: Запускаем мониторинг на 10 секунд
        DirectoryMonitor monitor(test_monitor_dir);

        // Добавляем еще один скрипт во время работы мониторинга
        thread add_script_thread([test_monitor_dir]() {
            sleep(3);
            cout << "[ДОБАВЛЕНИЕ СКРИПТА] Создаю дополнительный скрипт..." << endl;
            string extra_script = test_monitor_dir + "/extra_script.sh";
            FILE* f = fopen(extra_script.c_str(), "w");
            if (f) {
                fprintf(f, "#!/bin/bash\necho \"Дополнительный скрипт выполнен!\"\n");
                fclose(f);
                chmod(extra_script.c_str(), 0755);
                cout << "[ДОБАВЛЕНИЕ СКРИПТА] Дополнительный скрипт создан!" << endl;
            }
            });

        monitor.startMonitoringLimited(10);  // Мониторим 10 секунд

        add_script_thread.join();

        cout << endl;
        cout << "=== ПРОВЕРКА КАТАЛОГА МОНИТОРИНГА ===" << endl;
        string cmd2 = "ls -la " + test_monitor_dir;
        system(cmd2.c_str());

    }
    catch (const exception& e) {
        cerr << "Ошибка: " << e.what() << endl;
        return 1;
    }

    cout << endl;
    cout << "=== ВСЕ ЗАДАНИЯ ВЫПОЛНЕНЫ! ===" << endl;

    // Финальная проверка
    cout << endl;
    cout << "=== ФИНАЛЬНАЯ ПРОВЕРКА ===" << endl;
    cout << "Итоговое состояние каталогов:" << endl;
    string cmd3 = "ls -la " + test_monitor_dir;
    string cmd4 = "ls -la " + dest_dir;
    system(cmd3.c_str());
    system(cmd4.c_str());

    return 0;
}
