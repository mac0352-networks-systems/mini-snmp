// manager.cpp: main code that send requests to agents to collect data.

#include "log.h"
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <cctype>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <cstring>
#include <chrono>

using namespace std;
using internal_clock = chrono::steady_clock;

#define PORT 8080

struct Agent
{
    int connection;
    int agentSocket;
};

Logger logger("logs", "manager.log", Logger::OutputMode::BOTH);

vector<pthread_t> services;

volatile bool run = true;
bool send_request = false;

int managerSocket = -1;
int done_count = 0;
int total_agents;

pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t request_cond = PTHREAD_COND_INITIALIZER;


/**
 * @brief Signal handler function to gracefully terminate the manager process. 
 * @param sig The signal number that triggered the handler.
 */
void func_sig(int sig)
{
    run = 0;
    if (managerSocket != -1) 
    {
        close(managerSocket);
    }
    pthread_mutex_lock(&request_mutex);
    pthread_cond_broadcast(&request_cond);
    pthread_mutex_unlock(&request_mutex);
}

/**
 * @brief Thread of the manager to attend a agent connected
 * @param A Agent Struct that connected to the manager.
 */
void *request_service(void *arg)
{

    Agent *agentinfo = (Agent *)arg;

    int connection = agentinfo->connection;
    int agentSocket = agentinfo->agentSocket;

    logger.info("Agente " + to_string(connection) + " iniciou sessão de atendimento.");

    char buffer[1024];
    char *message = NULL;
    int bytesReceived, bytesTotal = 0;
    bool disconnected = false, connection_error = false;

    while(run){

        pthread_mutex_lock(&request_mutex);

        //thread responsavel pelo agente aguarda comando de fazer request
        while (!send_request && run) {
            pthread_cond_wait(&request_cond, &request_mutex);
        }

        if (!run) {
            pthread_mutex_unlock(&request_mutex);
            break;
        }

        pthread_mutex_unlock(&request_mutex);

        //faz o request
        string request = "GET INFO\n";

        send(agentSocket, request.c_str(), request.length(), 0);


        //loop de receber a informacao requisitada
        while(true)
        {
            bytesReceived = recv(agentSocket, buffer, sizeof(buffer), 0);
        
            if(bytesReceived == 0)
            {
                disconnected = true;
                break;
            }

            if (bytesReceived < 0){
                logger.error("Erro 100: Erro ao receber mensagem do agente " + to_string(connection) + ".");
            connection_error = true;
                break;
            }

            message = (char *) realloc(message, bytesTotal + bytesReceived);
            memcpy(message + bytesTotal, buffer, bytesReceived);
            bytesTotal += bytesReceived;

            if(memchr(buffer, '\n', bytesReceived)) break;
        }
        
        if(disconnected || connection_error) break;
        
        message = (char *) realloc(message, bytesTotal+1);
        message[bytesTotal-1] = '\0';
        string command(message);
        logger.debug("Mensagem do agente " + to_string(connection) + " do socket: " + command);

        /*
        string response = command_parser(message, agentSocket);
        string commandLabel = command_name(command);

        if (is_error_response(response))
                logger.error("Agente " + to_string(connection) + " -> " + commandLabel + " | " + trim_newline(response));
        else
                logger.info("Agente " + to_string(connection) + " -> " + commandLabel + " | " + trim_newline(response));

        string responseToSend = response + "\n";
        if (send(agentSocket, responseToSend.c_str(), responseToSend.length(), 0) < 0)
        {
                logger.error("Erro 100: Erro ao enviar resposta para Agente " + to_string(connection) + ".");
                break;
        }
        */

        //terminou o request

        pthread_mutex_lock(&request_mutex);

        done_count++;

        if (done_count == total_agents) {
            pthread_cond_signal(&request_cond); // acorda manager
        }

        pthread_mutex_unlock(&request_mutex);

        free(message);
        message = NULL;
        bytesTotal = 0;
    }
    
    /*
    int releasedCount = database.releaseAllByOwner(agentSocket);
    if (releasedCount > 0)
        logger.info("Agente " + to_string(connection) + " desconectou e liberou " + to_string(releasedCount) + " recurso(s)");
    */

    close(agentSocket);
    logger.info("Agente " + to_string(connection) + " desconectado do socket.");

    delete agentinfo;
    return NULL;
}

/**
 * @brief Thread of the manager to attend to new connections
 */
void *accept_agents(void *arg){

    int connection = 0;

    logger.info("Criando socket...");
    managerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (managerSocket < 0)
    {
        logger.error("Erro 100: Erro ao criar o socket.");
        return nullptr;
    }
    logger.info("Socket criado com sucesso!");

    sockaddr_in managerAddress{};
    managerAddress.sin_family = AF_INET;
    managerAddress.sin_port = htons(PORT);
    managerAddress.sin_addr.s_addr = INADDR_ANY;

    logger.info("Associando socket à porta " + to_string(PORT) + "...");
    if (::bind(managerSocket, (struct sockaddr *)&managerAddress, sizeof(managerAddress)) < 0)
    {
        logger.error("Erro 100: Erro ao associar o socket à porta " + to_string(PORT) + ".");
        return nullptr;
    }
    logger.info("Socket associado à porta " + to_string(PORT) + " com sucesso!");

    logger.info("Colocando o socket no modo de escuta...");
    if (listen(managerSocket, 5) < 0)
    {
        logger.error("Erro ao colocar o socket no modo de escuta.");
        close(managerSocket);
        return nullptr;
    }

    logger.info("Socket colocado no modo de escuta com sucesso!");

    struct sigaction sigconfigs;
    sigconfigs.sa_handler = func_sig;
    sigemptyset(&sigconfigs.sa_mask);
    sigconfigs.sa_flags = 0;

    sigaction(SIGINT, &sigconfigs, NULL);

    while (run)
    {

        logger.info("Esperando próxima conexão no socket...");
        int agentSocket = accept(managerSocket, nullptr, nullptr);
        if (agentSocket < 0)
        {

            if (errno == EINTR)
                break;

            logger.error("Erro 100: Erro ao aceitar conexão " + to_string(connection) + " do Agente no socket.");
            close(managerSocket);
            return nullptr;
        }
        logger.info("Agente " + to_string(connection) + " conectado no socket com sucesso! Pronto para receber mensagens");

        services.emplace_back();

        Agent *agentinfo = new Agent;
        agentinfo->connection = connection;
        agentinfo->agentSocket = agentSocket;

        pthread_create(&services.back(), NULL, request_service, agentinfo);
        connection++;
        
        pthread_mutex_lock(&request_mutex);
        total_agents = connection;
        pthread_mutex_unlock(&request_mutex);
    }
    close(managerSocket);
    logger.info("Socket fechado com sucesso! Servidor encerrado");
    return nullptr;
}

int main()
{
    //thread para lidar com novos agentes
    pthread_t listener;
    pthread_create(&listener, NULL, accept_agents, NULL);
    
    auto past = internal_clock::now();

    while (run) {
        auto current = internal_clock::now();

        if (current - past >= std::chrono::seconds(5)) {
            past = current;

            pthread_mutex_lock(&request_mutex);

            send_request = true;
            done_count = 0;

            pthread_cond_broadcast(&request_cond);

            // espera todo mundo terminar
            while (done_count < total_agents && run) {
                pthread_cond_wait(&request_cond, &request_mutex);
            }

            // reseta
            send_request = false;

            pthread_mutex_unlock(&request_mutex);

            //faz o print com informacoes atualizadas
            
        }
    }
    return 0;
}
