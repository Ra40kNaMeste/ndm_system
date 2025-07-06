#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <nlohmann/json.hpp>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <netlink/socket.h>

// Константы из ядерного модуля
constexpr char FAMILY_NAME[] = "NDM_Family";
constexpr int FAMILY_VERSION = 1;
constexpr int CALCULATE_FROM_JSON = 0;
constexpr int DATA = 1;

class NetlinkError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class NDMClient {
public:
    NDMClient() {
        sock.reset(nl_socket_alloc());
        if (!sock) {
            throw NetlinkError("Failed to allocate netlink socket");
        }

        if (genl_connect(sock.get()) < 0) {
            throw NetlinkError("Failed to connect to generic netlink");
        }

        family_id = genl_ctrl_resolve(sock.get(), FAMILY_NAME);
        if (family_id < 0) {
            throw NetlinkError("Failed to resolve NDM_Family");
        }
    }

    int send_json_request(const nlohmann::json& json_data, std::string& res) {
        auto msg = std::unique_ptr<nl_msg, decltype(&nlmsg_free)>(
            nlmsg_alloc(), &nlmsg_free);
        
        if (!msg) {
            return 1;
        }

        // Формируем заголовок
        if (!genlmsg_put(msg.get(), NL_AUTO_PORT, NL_AUTO_SEQ, family_id, 0, 0, 
                        CALCULATE_FROM_JSON, FAMILY_VERSION)) {
            return 2;
        }

        // Добавляем JSON данные как атрибут
        std::string json_str = json_data.dump();
        if (nla_put_string(msg.get(), DATA, json_str.c_str()) < 0) {
            return 3;
        }



        // --- Здесь принимаем ответ и вытаскиваем DATA ---

        // буфер для результата
        std::string result_data;

        // коллбэк
        auto cb = nl_cb_alloc(NL_CB_DEFAULT);
        if (!cb) {
            return 5;
        }

        nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM,
            [](struct nl_msg *msg, void *arg) -> int {
                std::string* out = static_cast<std::string*>(arg);

                struct nlmsghdr *nlh = nlmsg_hdr(msg);
                struct genlmsghdr *genlh = static_cast<genlmsghdr*>(nlmsg_data(nlh));
                struct nlattr *attrs[DATA + 1] = {};

                // разбираем атрибуты
                genlmsg_parse(nlh, 0, attrs, DATA, nullptr);

                if (attrs[DATA]) {
                    *out = nla_get_string(attrs[DATA]);
                }

                return NL_OK;
            },
            &result_data);

        // Отправляем сообщение
        std::cout << "Sending JSON: " << json_data.dump(2) << std::endl;
        if (nl_send_auto(sock.get(), msg.get()) < 0) {
            return 4;
        }
        // ждём ответ
        if (nl_recvmsgs(sock.get(), cb) < 0) {
            nl_cb_put(cb);
            return 6;
        }

        nl_cb_put(cb);
        res = result_data;
        return 0;
    }
private:
    std::unique_ptr<nl_sock, decltype(&nl_socket_free)> sock{nullptr, &nl_socket_free};
    int family_id;
};
int main() {
    //Исключения мы не ловим т.к. задание
    NDMClient client;

    while (true) {
        std::string action;
        std::cout << "Выберите действие:\n1) add\n2) sub\n3) mul\n4) exit\n";

        int act;
        while (!(std::cin >> act)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Это не integer. Попробуйте ещё раз: ";
        }
        bool is_exited = false;
        switch (act) {
            case 1:
                action = "add";
                break;
            case 2:
                action = "sub";
                break;
            case 3:
                action = "mul";
                break;
            case 4:
                std::cout << "Закрытие приложения..." << std::endl;
                is_exited = true;
                break;
            default:
                std::cout << "Действие не найдено. Выберите из add, sub, mul, or exit." << std::endl;
                continue;
        }
        if(is_exited) break;

        int argument_1, argument_2;
        std::cout << "Напишите первое целое число: ";
        while (!(std::cin >> argument_1)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Это не integer. Попробуйте ещё раз: ";
        }

        std::cout << "Напишите второе целое число: ";
        while (!(std::cin >> argument_2)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Это не integer. Попробуйте ещё раз: ";
        }

        // flush leftover newline
        std::cin.ignore(10000, '\n');

        // Build JSON
        nlohmann::json request = {
            {"action", action},
            {"argument_1", argument_1},
            {"argument_2", argument_2}
        };

        // Send
        std::cout << "\nОтправка сообщения к модулю\n";
        std::string response;
        int ret = client.send_json_request(request, response);
        if(ret){
            std::cout << "Упс. Что-то пошло не так. Код ошибки: " << ret << std::endl;
        }
        else{
            std::cout << "Сервер ответил: " << response << std::endl;
        }
        
    }

    return 0;
}