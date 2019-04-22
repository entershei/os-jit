#include <iostream>
#include <vector>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


const std::string HELP = R"SEQ(This program calculate sum of two numbers.
Usage:
    -help                       - usage this program
    -see                        - print current two numbers
    -calc                       - print sum of current two numbers
    -change [number1] [number2] - change current numbers and print sum, numbers should be [-2^31 ... 2^31-1]
    -exit                       - exit this program
)SEQ";

const long long MAX_INT = 2147483647;
const long long MIN_INT = -2147483648;

char const *FILE_NAME = "sum.o";
const size_t OFFSET_FOR_CALL = 0x40;
const size_t OFFSET_FOR_CHANGE1 = 0x7;
const size_t OFFSET_FOR_CHANGE2 = 0xe;
const int PAGE_SIZE_FOR_MAP = 4096;

const std::string str_help = "-help",
        str_see = "-see",
        str_calc = "-calc",
        str_exit = "-exit",
        str_change = "-change";

bool is_argument(const std::string &t, const std::string &s) {
    if (s.size() > t.size()) {
        return false;
    }

    return t.substr(0, s.size()) == s;
}

std::vector<std::string> delete_spaces(const std::string &s) {
    std::vector<std::string> ret;

    std::string cur;
    int pos = 0;

    while (pos < s.size()) {
        if (s[pos] != ' ') {
            cur += s[pos];
        } else if (!cur.empty()) {
            ret.push_back(cur);
            cur = "";
        }

        ++pos;
    }

    if (!cur.empty()) {
        ret.push_back(cur);
    }

    return ret;
}

bool is_number(const std::string &s) {
    for (char c : s) {
        if (c < '0' || '9' < c) {
            return false;
        }
    }

    return true;
}

int read_int(const std::string &s) {
    int ret = 0;

    for (char c : s) {
        int numeral = c - '0';
        if ((long long) (ret) + numeral > MAX_INT || (long long) (ret) + numeral < MIN_INT) {
            throw std::invalid_argument("Number should be [-2^31 ... 2^31-1]");
        }

        ret = ret * 10 + numeral;
    }

    return ret;
}

std::pair<int, int> read_numbs(const std::string &s) noexcept(false) {
    std::vector<std::string> after_split = delete_spaces(s);

    if (after_split.size() != 2 || !is_number(after_split[0]) || !is_number(after_split[1])) {
        throw std::invalid_argument("Expected two numbers");
    }

    return {read_int(after_split[0]), read_int(after_split[1])};
}

struct fd_t {
    fd_t(char const *file_name) {
        fd = open(file_name, O_RDONLY);
    }

    ~fd_t() {
        if (fd != -1) {
            int ret_close = close(fd);
        }
    }

    int get_fd() const {
        return fd;
    }

private:
    int fd;
};

struct jit_function {
    jit_function() {
        has_error = true;
        numb1 = 1;
        numb2 = 2;

        struct stat stat_info;
        if (stat(FILE_NAME, &stat_info) == -1) {
            return;
        }

        size_t file_size = static_cast<size_t>(stat_info.st_size);

        char buffer[PAGE_SIZE_FOR_MAP];

        fd_t fd = fd_t(FILE_NAME);
        if (fd.get_fd() == -1) {
            return;
        }

        if (read(fd.get_fd(), buffer, file_size) == -1) {
            return;
        }

        ptr_func = mmap(nullptr, PAGE_SIZE_FOR_MAP, PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (ptr_func == MAP_FAILED) {
            return;
        }

        memcpy(ptr_func, buffer + OFFSET_FOR_CALL, file_size - OFFSET_FOR_CALL);

        if (mprotect(ptr_func, PAGE_SIZE_FOR_MAP, PROT_NONE) == -1) {
            return;
        }

        has_error = false;
    }

    ~jit_function() {
        if (munmap(ptr_func, PAGE_SIZE_FOR_MAP) == -1) {
            std::cerr << strerror(errno) << std::endl;
        }
    }

    int get_sum() const {
        if (mprotect(ptr_func, PAGE_SIZE_FOR_MAP, PROT_EXEC | PROT_READ) == -1) {
            throw std::runtime_error("Can't execute function");
        }

        typedef int (*func)();

        func function = reinterpret_cast<func>(reinterpret_cast<size_t>(ptr_func));
        int result =  function();

        if (mprotect(ptr_func, PAGE_SIZE_FOR_MAP, PROT_NONE) == -1) {
            std::cerr << "Can't change read rights" << std::endl;
        }

        return result;
    }

    void change_numbs(int num1, int num2) {
        numb1 = num1;
        numb2 = num2;

        if (mprotect(ptr_func, PAGE_SIZE_FOR_MAP, PROT_WRITE) == -1) {
            std::cerr << "Can't change numbers." << std::endl;
            return;
        }

        change_value(OFFSET_FOR_CHANGE1, numb1);
        change_value(OFFSET_FOR_CHANGE2, numb2);

        if (mprotect(ptr_func, PAGE_SIZE_FOR_MAP, PROT_NONE) == -1) {
            std::cerr << "Can't change write rights." << std::endl;
        }
    }

    std::pair<int, int> get_cur_numbers() const {
        return {numb1, numb2};
    }

    bool get_has_error() const {
        return has_error;
    }

private:
    void *ptr_func;
    bool has_error;
    int numb1;
    int numb2;

    void change_value(size_t offset, int value) {
        void* ptr_change = reinterpret_cast<void *>(reinterpret_cast<size_t>(ptr_func) + offset);
        memcpy(ptr_change, &value, sizeof(int));
    }
};

int main() {
    std::cout << HELP << std::endl;

    std::string s;

    jit_function jit;

    if (jit.get_has_error()) {
        std::cerr << strerror(errno) << std::endl;
    }

    while (getline(std::cin, s)) {
        if (s.empty()) {
            continue;
        } else if (is_argument(s, str_help)) {
            std::cout << HELP << std::endl;
        } else if (is_argument(s, str_exit)) {
            return 0;
        } else if (is_argument(s, str_see)) {
            auto numbs = jit.get_cur_numbers();
            std::cout << numbs.first << " " << numbs.second << std::endl;
        } else if (is_argument(s, str_calc)) {
            try {
                auto sum = jit.get_sum();
                std::cout << sum << std::endl;
            } catch (const std::runtime_error &e) {
                std::cerr << e.what() << std::endl;
            }
        } else if (is_argument(s, str_change)) {
            std::pair<int, int> numbs;

            try {
                numbs = read_numbs(s.substr(str_change.size()));
            } catch (const std::invalid_argument &e) {
                std::cerr << e.what() << std::endl << HELP;
                continue;
            }

            jit.change_numbs(numbs.first, numbs.second);

            try {
                auto sum = jit.get_sum();
                std::cout << sum << std::endl;
            } catch (const std::runtime_error &e) {
                std::cerr << e.what() << std::endl;
            }
        } else {
            std::cout << "Input is incorrect.\n" << HELP << std::endl;
        }
    }

    return 0;
}