// agent.cpp: receive requests from menager and return current data.

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include "log.h"
#include <cstring>

#ifdef __linux__
#include <sys/sysinfo.h>   // RAM
#include <unistd.h>
#include <fstream>         // /proc
#include <sstream>

#elif __APPLE__
#include <sys/sysctl.h>    // CPU + RAM
#include <mach/mach.h>     // RAM detalhada
#endif

using namespace std;

double get_cpu_usage() 
{
#ifdef __linux__
    static long prev_idle = 0, prev_total = 0;

    std::ifstream file("/proc/stat");
    std::string cpu;

    long user, nice, system, idle, iowait, irq, softirq, steal;

    file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    long idle_time = idle + iowait;
    long total = user + nice + system + idle + iowait + irq + softirq + steal;

    long diff_idle = idle_time - prev_idle;
    long diff_total = total - prev_total;

    prev_idle = idle_time;
    prev_total = total;

    if (diff_total == 0) return 0.0;

    return (1.0 - (double)diff_idle / diff_total) * 100.0;

#elif defined(__APPLE__)
    static long prev_user = 0, prev_system = 0, prev_idle = 0;

    long user, system, idle;
    get_cpu_times(user, system, idle);

    long prev_total = prev_user + prev_system + prev_idle;
    long total = user + system + idle;

    long diff_total = total - prev_total;
    long diff_idle = idle - prev_idle;

    prev_user = user;
    prev_system = system;
    prev_idle = idle;

    if (diff_total == 0) return 0.0;

    return (1.0 - (double)diff_idle / diff_total) * 100.0;

#else
    return 0.0;

#endif
}


double get_memory_usage()
{
    #if defined(__linux__)
        FILE* fp = fopen("/proc/self/status", "r");
        if (fp) {
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                if (strncmp(line, "VmRSS:", 6) == 0) {
                    double value_kb;
                    // Lendo o valor numérico como double para manter a precisão
                    sscanf(line + 6, "%lf", &value_kb);
                    fclose(fp);
                    return value_kb / 1024.0;
                }
            }
            fclose(fp);
        }

    #elif defined(__APPLE__) && defined(__MACH__)
        struct mach_task_basic_info info;
        mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;

        if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
            // Convertendo bytes diretamente para double em MB
            return (double)info.resident_size / (1024.0 * 1024.0);
        }
    #endif
    return -1.0; 
}


int main()
{
    Logger logger("logs", "agent.log", Logger::OutputMode::BOTH);

    logger.info("Criando socket...");
    int agentSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (agentSocket < 0)
    {
        logger.error("Erro 100: Erro ao criar o socket.");
        return 1;
    }
    logger.info("Socket criado com sucesso!");

    sockaddr_in managerAddress{};
    managerAddress.sin_family = AF_INET;
    managerAddress.sin_port = htons(8080);
    managerAddress.sin_addr.s_addr = INADDR_ANY;

    logger.info("Conectando ao manager...");
    if (connect(agentSocket, (struct sockaddr *)&managerAddress, sizeof(managerAddress)) < 0)
    {
        logger.error("Erro 100: Erro ao conectar ao manager.");
        close(agentSocket);
        return 1;
    }
    logger.info("Conectado com sucesso!");

    string message;
    char buffer[1024];
    int bytesReceived;

    while (true)
    {
        bytesReceived = recv(agentSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0)
        {
            buffer[bytesReceived] = '\0';
            string response(buffer);
            if (response.rfind("Erro", 0) == 0)
            {
                logger.error("Resposta do manager: " + response);
                cout << "Foi recebido um error do manager: " << response << endl;
            }
            else if(response == "GET INFO\n"){ 
                cout << "Foi recebido um request do manager:" << response << endl;
                double cpu_value = get_cpu_usage();
                double memory_value = get_memory_usage();
                string data = "CPU(%): " + to_string(cpu_value) + "% / " + 
                              "MEMORY(%) "+ to_string(memory_value) + "%" + '\n';
                send(agentSocket, data.c_str(), data.length(), 0);
            }
            else
            {
                logger.info("Request do manager: " + response);
                cout << "Resposta do agente: " << response << endl;
            }
        }
        else if (bytesReceived == 0)
        {
            logger.info("Manager desconectado.");
            cout << "Manager desconectado." << endl;
            break;
        }
        else
        {
            logger.error("Erro 100: Erro ao receber resposta.");
            cout << "Erro 100: Erro ao receber resposta do manager." << endl;
            break;
        }
    }

    cout << "Encerrando agente..." << endl;
    logger.info("Fechando soquete...");
    close(agentSocket);
    logger.info("Socket fechado com sucesso! Agente encerrado.");

    return 0;
}
