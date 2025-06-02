#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <optional>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <sstream>

struct InstrucaoInput { // armazena a instrução lida no arquivo de entrada
    std::string d_operacao;
    std::string r_reg;
    std::string s_reg_or_imm;
    std::string t_reg_or_label;
};

struct ConfigSimulador {
    int numInstrucoes = 0;
    std::map<std::string, int> ciclos;
    std::map<std::string, int> unidades;
    std::map<std::string, int> unidadesMem;
};

struct InstrucaoDetalhes { //campos da instrução
    std::string operacao;
    std::string registradorR;
    std::string registradorS;
    std::string registradorT;
};

struct EstadoInstrucao { //guarda o estado de uma determinada instrução durante a execução do algoritmo
    InstrucaoDetalhes instrucao;
    int posicao;
    std::optional<int> issue;
    std::optional<int> exeCompleta;
    std::optional<int> write;
    bool busy = false;

    EstadoInstrucao() : posicao(0), busy(false) {}
    EstadoInstrucao(InstrucaoDetalhes details, int pos)
        : instrucao(std::move(details)), posicao(pos), busy(false) {}
};

struct UnidadeFuncional {
    std::optional<InstrucaoDetalhes> instrucao_details;
    EstadoInstrucao* estadoInstrucaoOriginal = nullptr;
    std::string tipoUnidade;
    std::optional<int> tempo;
    std::string nome;
    bool ocupado = false;
    std::optional<std::string> operacao;
    std::optional<std::string> vj;
    std::optional<std::string> vk;
    std::optional<std::string> qj;
    std::optional<std::string> qk;
};

struct UnidadeFuncionalMemoria {
    std::optional<InstrucaoDetalhes> instrucao_details;
    EstadoInstrucao* estadoInstrucaoOriginal = nullptr;
    std::string tipoUnidade;
    std::optional<int> tempo;
    std::string nome;
    bool ocupado = false;
    std::optional<std::string> qi;
    std::optional<std::string> qj;
    std::optional<std::string> operacao;
    std::optional<std::string> endereco;
    std::optional<std::string> destino;
};

class Estado {
public:
    ConfigSimulador config;
    std::vector<EstadoInstrucao> estadoInstrucoes;
    std::map<std::string, UnidadeFuncional> unidadesFuncionais;
    std::map<std::string, UnidadeFuncionalMemoria> unidadesFuncionaisMemoria;
    int clock_cycle;
    std::map<std::string, std::optional<std::string>> estacaoRegistradores;

    Estado(const ConfigSimulador& cfg, const std::vector<InstrucaoInput>& instrucoes_input) // inicialização das instruções, registradores e unidades funcionais
        : config(cfg), clock_cycle(0) {
        for (int i = 0; i < instrucoes_input.size(); ++i) {
            InstrucaoDetalhes details;
            details.operacao = instrucoes_input[i].d_operacao;
            details.registradorR = instrucoes_input[i].r_reg;
            details.registradorS = instrucoes_input[i].s_reg_or_imm;
            details.registradorT = instrucoes_input[i].t_reg_or_label;
            estadoInstrucoes.emplace_back(details, i);
        }
        this->config.numInstrucoes = instrucoes_input.size();

        for (const auto& pair : config.unidades) {
            const std::string& tipo = pair.first;
            int count = pair.second;
            for (int i = 0; i < count; ++i) {
                UnidadeFuncional uf;
                uf.nome = tipo + std::to_string(i + 1);
                uf.tipoUnidade = tipo;
                unidadesFuncionais[uf.nome] = uf;
            }
        }

        for (const auto& pair : config.unidadesMem) {
            const std::string& tipo = pair.first;
            int count = pair.second;
            for (int i = 0; i < count; ++i) {
                UnidadeFuncionalMemoria uf_mem;
                uf_mem.nome = tipo + std::to_string(i + 1);
                uf_mem.tipoUnidade = tipo;
                unidadesFuncionaisMemoria[uf_mem.nome] = uf_mem;
            }
        }

        for (int i = 0; i < 32; i += 2) {
            estacaoRegistradores["F" + std::to_string(i)] = std::nullopt;
        }
        for (int i = 0; i < 32; ++i) {
            estacaoRegistradores["R" + std::to_string(i)] = std::nullopt;
        }
    }

    EstadoInstrucao* getNovaInstrucao() { // retorna a próxima instrução que ainda foi emitida 
        for (auto& instr_state : estadoInstrucoes) {
            if (!instr_state.issue.has_value()) {
                return &instr_state;
            }
        }
        return nullptr;
    }

    std::string verificaUFInstrucao(const InstrucaoDetalhes& instr_details) { //mapeia a operação para a unidade funcional capaz de executá-la
        const std::string& op = instr_details.operacao;
        if (op == "ADDD" || op == "SUBD") return "Add";
        if (op == "MULTD") return "Mult";
        if (op == "DIVD") return "Div";
        if (op == "LD") return "Load";
        if (op == "SD") return "Store";
        if (op == "ADD" || op == "DADDUI" || op == "BEQ" || op == "BNEZ") return "Integer";
        std::cerr << "WARN: Unknown operation for UF check: " << op << std::endl;
        return "";
    }

    //getUFVazia: retorna ponteiro para a unidade funcional livre

    UnidadeFuncional* getFUVaziaArithInt(const std::string& tipoFU) { 
        for (auto& pair : unidadesFuncionais) {
            if (pair.second.tipoUnidade == tipoFU && !pair.second.ocupado) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    UnidadeFuncionalMemoria* getFUVaziaMem(const std::string& tipoFU) {
        for (auto& pair : unidadesFuncionaisMemoria) {
            if (pair.second.tipoUnidade == tipoFU && !pair.second.ocupado) {
                return &pair.second;
            }
        }
        return nullptr;
    }

    int getCiclos(const InstrucaoDetalhes& instr_details) { //retorna o número de ciclos que a instrução gasta
        const std::string& op_type = verificaUFInstrucao(instr_details);
        if (config.ciclos.count(op_type)) {
            return config.ciclos.at(op_type);
        }
        std::cerr << "Error: Cycle count not found for FU type '" << op_type
                  << "' derived from operation '" << instr_details.operacao << "'" << std::endl;
        return 1;
    }


    void alocaFU(UnidadeFuncional& uf, const InstrucaoDetalhes& instr_details, EstadoInstrucao& estado_instr_orig) { // aloca uma unidade funcional para a instrução
        uf.instrucao_details = instr_details;
        uf.estadoInstrucaoOriginal = &estado_instr_orig;
        uf.tempo = getCiclos(instr_details) + 1;
        uf.ocupado = true;
        uf.operacao = instr_details.operacao;
        uf.vj = std::nullopt;
        uf.vk = std::nullopt;
        uf.qj = std::nullopt;
        uf.qk = std::nullopt;

        std::string src_reg_j_name, src_reg_k_name;

        if (instr_details.operacao == "ADDD" || instr_details.operacao == "SUBD") {
            if (uf.tempo.has_value() && uf.tempo.value() > 0) {
                uf.tempo = uf.tempo.value() - 1;
            }
        }

        if (instr_details.operacao == "BNEZ" || instr_details.operacao == "BEQ") {
            src_reg_j_name = instr_details.registradorR;
            src_reg_k_name = instr_details.registradorS;
        } else {
            src_reg_j_name = instr_details.registradorS;
            src_reg_k_name = instr_details.registradorT;
        }

        auto setup_operand = [&](std::optional<std::string>& v_val, std::optional<std::string>& q_val, const std::string& reg_name) {
            if (reg_name.empty()) {
                v_val = "N/A";
                return;
            }
            bool is_immediate = true;
            if (reg_name.empty() || (!reg_name.empty() && (reg_name[0] == 'F' || reg_name[0] == 'R'))) {
                is_immediate = false;
                for(char c : reg_name)
                    if (reg_name.length() > 1 && isalpha(c) && c != 'F' && c != 'R')
                        is_immediate = true;
            }

            if (is_immediate && !(reg_name[0] == 'F' || reg_name[0] == 'R')) {
                v_val = reg_name;
            } else if (estacaoRegistradores.count(reg_name)) {
                const auto& reg_status = estacaoRegistradores.at(reg_name);
                if (!reg_status.has_value() || reg_status.value().rfind("VAL(", 0) == 0) {
                    v_val = reg_status.has_value() ? reg_status.value() : reg_name;
                } else {
                    const std::string& producing_fu_name = reg_status.value();
                    if (unidadesFuncionais.count(producing_fu_name) || unidadesFuncionaisMemoria.count(producing_fu_name)) {
                        q_val = producing_fu_name;
                    } else {
                        v_val = producing_fu_name;
                    }
                }
            } else {
                v_val = reg_name;
            }
        };

        setup_operand(uf.vj, uf.qj, src_reg_j_name);
        setup_operand(uf.vk, uf.qk, src_reg_k_name);
    }

    void alocaFuMem(UnidadeFuncionalMemoria& uf_mem, const InstrucaoDetalhes& instr_details, EstadoInstrucao& estado_instr_orig) { // aloca uma unidade funcional para a instrução
        uf_mem.instrucao_details = instr_details;
        uf_mem.estadoInstrucaoOriginal = &estado_instr_orig;
        uf_mem.tempo = getCiclos(instr_details) + 1;
        uf_mem.ocupado = true;
        uf_mem.operacao = instr_details.operacao;
        uf_mem.endereco = instr_details.registradorS + "+" + instr_details.registradorT;
        uf_mem.destino = instr_details.registradorR;
        uf_mem.qi = std::nullopt;
        uf_mem.qj = std::nullopt;

        if (instr_details.operacao == "SD") {
            if (estacaoRegistradores.count(instr_details.registradorR)) {
                const auto& producing_fu_name_opt = estacaoRegistradores.at(instr_details.registradorR);
                if (producing_fu_name_opt.has_value()) {
                    const std::string& producing_fu_name = producing_fu_name_opt.value();
                    if (unidadesFuncionais.count(producing_fu_name) || unidadesFuncionaisMemoria.count(producing_fu_name)) {
                        uf_mem.qi = producing_fu_name;
                    }
                }
            }
        }

        if (estacaoRegistradores.count(instr_details.registradorT)) {
            const auto& producing_fu_name_opt = estacaoRegistradores.at(instr_details.registradorT);
            if (producing_fu_name_opt.has_value()) {
                const std::string& producing_fu_name = producing_fu_name_opt.value();
                if (unidadesFuncionais.count(producing_fu_name) || unidadesFuncionaisMemoria.count(producing_fu_name)) {
                    uf_mem.qj = producing_fu_name;
                }
            }
        }
    }

    void escreveEstacaoRegistrador(const InstrucaoDetalhes& instr_details, const std::string& ufNome) { //informa ao registrador final qual unidade funcional irá lhe entregar o resultado da operação
        if (instr_details.operacao != "SD" && instr_details.operacao != "BEQ" && instr_details.operacao != "BNEZ") {
            if (!instr_details.registradorR.empty()) {
                estacaoRegistradores[instr_details.registradorR] = ufNome;
            }
        }
    }

    

    void liberaUFEsperandoResultado(const std::string& nomeUFQueTerminou) { //libera dependeências que estavam esperando a liberação da unidade funcional
        std::string val_representation = "VAL(" + nomeUFQueTerminou + ")";

        for (auto& pair : unidadesFuncionais) {
            UnidadeFuncional& uf_esperando = pair.second;
            if (uf_esperando.ocupado) {
                bool qj_cleared_by_this_fu = false;
                bool qk_cleared_by_this_fu = false;

                if (uf_esperando.qj.has_value() && uf_esperando.qj.value() == nomeUFQueTerminou) {
                    uf_esperando.vj = val_representation;
                    uf_esperando.qj = std::nullopt;
                    qj_cleared_by_this_fu = true;
                }
                if (uf_esperando.qk.has_value() && uf_esperando.qk.value() == nomeUFQueTerminou) {
                    uf_esperando.vk = val_representation;
                    uf_esperando.qk = std::nullopt;
                    qk_cleared_by_this_fu = true;
                }

                if ((qj_cleared_by_this_fu || qk_cleared_by_this_fu) &&
                    !uf_esperando.qj.has_value() && !uf_esperando.qk.has_value()) {
                    if (uf_esperando.tempo.has_value() && uf_esperando.tempo.value() > 0) {
                        uf_esperando.tempo = uf_esperando.tempo.value() - 1;
                    }
                }
            }
        }

        for (auto& pair : unidadesFuncionaisMemoria) {
            UnidadeFuncionalMemoria& uf_mem_esperando = pair.second;
            if (uf_mem_esperando.ocupado) {
                bool dependency_resolved_by_this_fu_for_mem = false;

                if (uf_mem_esperando.qi.has_value() && uf_mem_esperando.qi.value() == nomeUFQueTerminou) {
                    uf_mem_esperando.qi = std::nullopt;
                    dependency_resolved_by_this_fu_for_mem = true;
                }
                else if (uf_mem_esperando.qj.has_value() && uf_mem_esperando.qj.value() == nomeUFQueTerminou) {
                    uf_mem_esperando.qj = std::nullopt;
                    dependency_resolved_by_this_fu_for_mem = true;
                }

                if (dependency_resolved_by_this_fu_for_mem) {
                    if (uf_mem_esperando.tempo.has_value() && uf_mem_esperando.tempo.value() > 0) {
                        uf_mem_esperando.tempo = uf_mem_esperando.tempo.value() - 1;
                    }
                }
            }
        }
    }

    //limpeza das instruções e mudança dos status da unidades funcionais

    void desalocaUFMem(UnidadeFuncionalMemoria& uf_mem) {
        uf_mem.instrucao_details = std::nullopt;
        uf_mem.estadoInstrucaoOriginal = nullptr;
        uf_mem.tempo = std::nullopt;
        uf_mem.ocupado = false;
        uf_mem.operacao = std::nullopt;
        uf_mem.endereco = std::nullopt;
        uf_mem.destino = std::nullopt;
        uf_mem.qi = std::nullopt;
        uf_mem.qj = std::nullopt;
    }

    void desalocaUF(UnidadeFuncional& uf) {
        uf.instrucao_details = std::nullopt;
        uf.estadoInstrucaoOriginal = nullptr;
        uf.tempo = std::nullopt;
        uf.ocupado = false;
        uf.operacao = std::nullopt;
        uf.vj = std::nullopt;
        uf.vk = std::nullopt;
        uf.qj = std::nullopt;
        uf.qk = std::nullopt;
    }

    bool verificaSeJaTerminou() { //retorna true se todas as instruções do arquivo de entrada tiverem escrito seus resultados
        if (estadoInstrucoes.empty()) return true;
        for (const auto& instr_state : estadoInstrucoes) {
            if (!instr_state.write.has_value()) {
                return false;
            }
        }
        return true;
    }

    void issueNovaInstrucao() { //busca a nova instrução, procura uma unidade funcional para alocá-la e marca o ciclo de emissão da instrução
        EstadoInstrucao* nova_instr_estado = getNovaInstrucao();
        if (nova_instr_estado) {
            std::string tipoFU_str = verificaUFInstrucao(nova_instr_estado->instrucao);
            if (tipoFU_str.empty()){
                std::cerr << "ERROR: Cannot determine FU type for " << nova_instr_estado->instrucao.operacao << std::endl;
                return;
            }

            if (tipoFU_str == "Load" || tipoFU_str == "Store") {
                UnidadeFuncionalMemoria* uf_para_usar = getFUVaziaMem(tipoFU_str);
                if (uf_para_usar) {
                    alocaFuMem(*uf_para_usar, nova_instr_estado->instrucao, *nova_instr_estado);
                    nova_instr_estado->issue = clock_cycle;
                    if (uf_para_usar->instrucao_details.has_value() &&
                        uf_para_usar->instrucao_details.value().operacao != "SD") {
                        escreveEstacaoRegistrador(nova_instr_estado->instrucao, uf_para_usar->nome);
                    }
                }
            } else {
                UnidadeFuncional* uf_para_usar = getFUVaziaArithInt(tipoFU_str);
                if (uf_para_usar) {
                    alocaFU(*uf_para_usar, nova_instr_estado->instrucao, *nova_instr_estado);
                    nova_instr_estado->issue = clock_cycle;
                    const auto& op = nova_instr_estado->instrucao.operacao;
                    if (op != "BEQ" && op != "BNEZ") {
                        escreveEstacaoRegistrador(nova_instr_estado->instrucao, uf_para_usar->nome);
                    }
                }
            }
        }
    }

    void executaInstrucao() { //verifica se a unidade funcional não tem dependências, decrementa o tempo restante de execução e marca o ciclo de término da instrução
        for (auto& pair : unidadesFuncionaisMemoria) {
            UnidadeFuncionalMemoria& uf_mem = pair.second;
            if (uf_mem.ocupado && !uf_mem.qi.has_value() && !uf_mem.qj.has_value()) {
                if (uf_mem.tempo.has_value()) {
                    if (uf_mem.tempo.value() > 0) {
                        uf_mem.tempo = uf_mem.tempo.value() - 1;
                        if (uf_mem.estadoInstrucaoOriginal)
                            uf_mem.estadoInstrucaoOriginal->busy = true;
                    }
                    if (uf_mem.tempo.value() == 0) {
                        if (uf_mem.estadoInstrucaoOriginal) {
                            uf_mem.estadoInstrucaoOriginal->exeCompleta = clock_cycle;
                            uf_mem.estadoInstrucaoOriginal->busy = false;
                            uf_mem.tempo = -1;
                        }
                    }
                }
            }
        }

        for (auto& pair : unidadesFuncionais) {
            UnidadeFuncional& uf = pair.second;
            if (uf.ocupado && uf.vj.has_value() && uf.vk.has_value() &&
                !uf.qj.has_value() && !uf.qk.has_value()) {
                if (uf.tempo.has_value()) {
                    if (uf.tempo.value() > 0) {
                        uf.tempo = uf.tempo.value() - 1;
                        if (uf.estadoInstrucaoOriginal)
                            uf.estadoInstrucaoOriginal->busy = true;
                    }
                    if (uf.tempo.value() == 0) {
                        if (uf.estadoInstrucaoOriginal) {
                            uf.estadoInstrucaoOriginal->exeCompleta = clock_cycle;
                            uf.estadoInstrucaoOriginal->busy = false;
                            uf.tempo = -1;
                        }
                    }
                }
            }
        }
    }

    void escreveInstrucao() { //registra o resultado da instrução em seu registrador de destino
        for (auto& pair : unidadesFuncionaisMemoria) {
            UnidadeFuncionalMemoria& uf_mem = pair.second;
            if (uf_mem.ocupado && uf_mem.tempo.has_value() && uf_mem.tempo.value() == -1 &&
                uf_mem.estadoInstrucaoOriginal && !uf_mem.estadoInstrucaoOriginal->write.has_value() &&
                uf_mem.estadoInstrucaoOriginal->exeCompleta.has_value() &&
                uf_mem.estadoInstrucaoOriginal->exeCompleta.value() < clock_cycle) {
                uf_mem.estadoInstrucaoOriginal->write = clock_cycle;
                if (uf_mem.instrucao_details.has_value()) {
                    const auto& instr_d = uf_mem.instrucao_details.value();
                    if (instr_d.operacao != "SD") {
                        if (estacaoRegistradores.count(instr_d.registradorR)) {
                            auto& reg_status = estacaoRegistradores.at(instr_d.registradorR);
                            if (reg_status.has_value() && reg_status.value() == uf_mem.nome) {
                                reg_status = "VAL(" + uf_mem.nome + ")";
                            }
                        }
                    }
                }
                liberaUFEsperandoResultado(uf_mem.nome);
                desalocaUFMem(uf_mem);
            }
        }

        for (auto& pair : unidadesFuncionais) {
            UnidadeFuncional& uf = pair.second;
            if (uf.ocupado && uf.tempo.has_value() && uf.tempo.value() == -1 &&
                uf.estadoInstrucaoOriginal && !uf.estadoInstrucaoOriginal->write.has_value() &&
                uf.estadoInstrucaoOriginal->exeCompleta.has_value() &&
                uf.estadoInstrucaoOriginal->exeCompleta.value() < clock_cycle) {
                uf.estadoInstrucaoOriginal->write = clock_cycle;
                if (uf.instrucao_details.has_value()) {
                    const auto& instr_d = uf.instrucao_details.value();
                    if (instr_d.operacao != "BEQ" && instr_d.operacao != "BNEZ") {
                        if (estacaoRegistradores.count(instr_d.registradorR)) {
                            auto& reg_status = estacaoRegistradores.at(instr_d.registradorR);
                            if (reg_status.has_value() && reg_status.value() == uf.nome) {
                                reg_status = "VAL(" + uf.nome + ")";
                            }
                        }
                    }
                }
                liberaUFEsperandoResultado(uf.nome);
                desalocaUF(uf);
            }
        }
    }

    bool executa_ciclo() { //exxecuta um ciclo completo
        clock_cycle++;
        issueNovaInstrucao();
        executaInstrucao();
        escreveInstrucao();
        return verificaSeJaTerminou();
    }

    void printEstadoDebug() const { //imprime o estado das instruções, unidades funcionais, memória e registradores
        std::cout << "\n--- Clock: " << clock_cycle << " ---" << std::endl;
        std::cout << "\n== Status das Instrucoes ==" << std::endl;
        std::cout << std::left << std::setw(5) << "#"
                  << std::setw(8) << "Instr"
                  << std::setw(5) << "R"
                  << std::setw(8) << "S"
                  << std::setw(8) << "T"
                  << std::setw(7) << "Issue"
                  << std::setw(7) << "Exec"
                  << std::setw(7) << "Write"
                  << std::setw(6) << "Busy" << std::endl;
        for (const auto& s : estadoInstrucoes) {
            std::cout << std::left << std::setw(5) << s.posicao
                      << std::setw(8) << s.instrucao.operacao
                      << std::setw(5) << s.instrucao.registradorR
                      << std::setw(8) << s.instrucao.registradorS
                      << std::setw(8) << s.instrucao.registradorT
                      << std::setw(7) << (s.issue.has_value() ? std::to_string(s.issue.value()) : "-")
                      << std::setw(7) << (s.exeCompleta.has_value() ? std::to_string(s.exeCompleta.value()) : "-")
                      << std::setw(7) << (s.write.has_value() ? std::to_string(s.write.value()) : "-")
                      << std::setw(6) << (s.busy ? "Sim" : "Nao") << std::endl;
        }

        std::cout << "\n== Estacoes de Reserva (Aritmeticas/Inteiro) ==" << std::endl;
        std::cout << std::left << std::setw(10) << "Nome"
                  << std::setw(8) << "Ocupado"
                  << std::setw(7) << "Tempo"
                  << std::setw(10) << "Op"
                  << std::setw(12) << "Vj"
                  << std::setw(12) << "Vk"
                  << std::setw(10) << "Qj"
                  << std::setw(10) << "Qk" << std::endl;
        for (const auto& pair : unidadesFuncionais) {
            const auto& uf = pair.second;
            std::cout << std::left << std::setw(10) << uf.nome
                      << std::setw(8) << (uf.ocupado ? "Sim" : "Nao")
                      << std::setw(7) << (uf.tempo.has_value() ? std::to_string(uf.tempo.value()) : "-")
                      << std::setw(10) << (uf.operacao.has_value() ? uf.operacao.value() : "-")
                      << std::setw(12) << (uf.vj.has_value() ? uf.vj.value() : "-")
                      << std::setw(12) << (uf.vk.has_value() ? uf.vk.value() : "-")
                      << std::setw(10) << (uf.qj.has_value() ? uf.qj.value() : "-")
                      << std::setw(10) << (uf.qk.has_value() ? uf.qk.value() : "-") << std::endl;
        }

        std::cout << "\n== Buffers de Load/Store (Memoria) ==" << std::endl;
        std::cout << std::left << std::setw(10) << "Nome"
                  << std::setw(8) << "Ocupado"
                  << std::setw(7) << "Tempo"
                  << std::setw(8) << "Op"
                  << std::setw(15) << "Endereco"
                  << std::setw(10) << "Dest/Src"
                  << std::setw(10) << "Qi"
                  << std::setw(10) << "Qj (Base)" << std::endl;
        for (const auto& pair : unidadesFuncionaisMemoria) {
            const auto& uf = pair.second;
            std::cout << std::left << std::setw(10) << uf.nome
                      << std::setw(8) << (uf.ocupado ? "Sim" : "Nao")
                      << std::setw(7) << (uf.tempo.has_value() ? std::to_string(uf.tempo.value()) : "-")
                      << std::setw(8) << (uf.operacao.has_value() ? uf.operacao.value() : "-")
                      << std::setw(15) << (uf.endereco.has_value() ? uf.endereco.value() : "-")
                      << std::setw(10) << (uf.destino.has_value() ? uf.destino.value() : "-")
                      << std::setw(10) << (uf.qi.has_value() ? uf.qi.value() : "-")
                      << std::setw(10) << (uf.qj.has_value() ? uf.qj.value() : "-") << std::endl;
        }

        std::cout << "\n== Status dos Registradores ==" << std::endl;
        bool first_reg = true;
        for (const auto& pair : estacaoRegistradores) {
            if (!first_reg) std::cout << ", ";
            std::cout << pair.first << ":" << (pair.second.has_value() ? pair.second.value() : "null");
            first_reg = false;
        }
        std::cout << "\n-----------------------------------------" << std::endl;
    }
};

std::string trim(const std::string& str) {
    const std::string whitespace = " \t\n\r\f\v";
    size_t start = str.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

bool parseInputFile(const std::string& filename, ConfigSimulador& out_config, std::vector<InstrucaoInput>& out_instructions) { //eitura do arquivo de entrada
    std::ifstream infile(filename);
    if (!infile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return false;
    }

    std::string line;
    enum class ParseState { NONE, CONFIG, INSTRUCTIONS };
    ParseState currentState = ParseState::NONE;

    while (std::getline(infile, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line == "CONFIG_BEGIN") {
            currentState = ParseState::CONFIG;
            continue;
        } else if (line == "CONFIG_END") {
            currentState = ParseState::NONE;
            continue;
        } else if (line == "INSTRUCTIONS_BEGIN") {
            currentState = ParseState::INSTRUCTIONS;
            continue;
        } else if (line == "INSTRUCTIONS_END") {
            currentState = ParseState::NONE;
            break;
        }

        std::stringstream ss(line);
        std::string keyword, param1, param2_str;
        int param2_val;

        if (currentState == ParseState::CONFIG) {
            ss >> keyword;
            if (keyword == "CYCLES") {
                ss >> param1 >> param2_val;
                out_config.ciclos[param1] = param2_val;
            } else if (keyword == "UNITS") {
                ss >> param1 >> param2_val;
                out_config.unidades[param1] = param2_val;
            } else if (keyword == "MEM_UNITS") {
                ss >> param1 >> param2_val;
                out_config.unidadesMem[param1] = param2_val;
            } else {
                std::cerr << "Warning: Unknown config keyword '" << keyword << "' in line: " << line << std::endl;
            }
        } else if (currentState == ParseState::INSTRUCTIONS) {
            InstrucaoInput instr;
            ss >> instr.d_operacao >> instr.r_reg >> instr.s_reg_or_imm >> instr.t_reg_or_label;
            if (!instr.d_operacao.empty()) {
                out_instructions.push_back(instr);
            } else {
                std::cerr << "Warning: Could not parse instruction line: " << line << std::endl;
            }
        }
    }
    out_config.numInstrucoes = out_instructions.size();
    return true;
}

int main(int argc, char* argv[]) { //leitura do arquivo principal, criação do simulador, decisão de execução do algoritmo e executa todas as instruções
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file.txt>" << std::endl;
        return 1;
    }
    std::string filename = argv[1];

    ConfigSimulador config;
    std::vector<InstrucaoInput> instructions;

    if (!parseInputFile(filename, config, instructions)) {
        return 1;
    }

    if (instructions.empty()) {
        std::cout << "No instructions found in the input file." << std::endl;
        return 0;
    }

    Estado simulador(config, instructions);
    bool terminou = false;
    int cycle_limit = 200;
    int current_cycle = 0;

    std::cout << "Simulacao Iniciada. Pressione Enter para avancar ciclo a ciclo, ou 'r' para rodar ate o fim." << std::endl;
    simulador.printEstadoDebug();

    char step_mode = 's';
    if (argc > 2 && std::string(argv[2]) == "run") {
        step_mode = 'r';
    }

    while (!terminou && current_cycle < cycle_limit) {
        if (step_mode == 's') {
            std::cout << "Pressione Enter para o proximo ciclo (Clock: " << simulador.clock_cycle + 1 << ") ou 'r' para rodar ate o fim: ";
            char c = std::cin.get();
            if (c == 'r' || c == 'R') {
                step_mode = 'r';
            }
            if (c != '\n' && c != EOF) {
                std::string dummy;
                std::getline(std::cin, dummy);
            }
        }
        terminou = simulador.executa_ciclo();
        current_cycle = simulador.clock_cycle;
        simulador.printEstadoDebug();

        if (terminou) {
            std::cout << "\n== Simulacao Concluida em " << simulador.clock_cycle << " ciclos. ==" << std::endl;
        }
    }

    if (!terminou && current_cycle >= cycle_limit) {
        std::cout << "\n== Simulacao Parada: Limite de ciclos (" << cycle_limit << ") atingido. ==" << std::endl;
    }

    std::cout << "\n== Estado Final dos Registradores Usados/Definidos ==" << std::endl;
    bool first_reg = true;
    for (const auto& pair : simulador.estacaoRegistradores) {
        if (!first_reg) std::cout << "; ";
        std::cout << pair.first << ": " << (pair.second.has_value() ? pair.second.value() : "initial/unused");
        first_reg = false;
    }
    std::cout << std::endl;

    return 0;
}
