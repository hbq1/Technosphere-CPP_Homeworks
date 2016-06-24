#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <iostream>
#include <vector>




class Shell 
{
    std::vector<std::string> commands;
    
public:
    Shell() {}
    ~Shell() {}

//control definitions
//
    bool is_redirect(char s)
    {   
        return s == '<' || s == '>';
    }

    bool is_space(char s)
    {
        return s == ' ' || s == '\t' || s == '\n';
    }

    bool is_control(char s) 
    {
        return s == '|' || s == '&' || s == ';' || is_redirect(s);
    }
    bool is_redirect(std::string s)
    {   
        return s == "<" || s == ">" || s == ">>";
    }

    bool is_logic_cmd(std::string s)
    {
        return s == "&&" || s == "||" || s == ";";
    }

    bool is_space(std::string s)
    {
        return s == " " || s == "\t" || s =="\n";
    }

    bool is_control(std::string s) 
    {
        return s == "|" || s == "&" || s == ";" || is_logic_cmd(s) || is_redirect(s);
    }

//
//ok defined

    bool set_cmd_line(std::string line)
    {
        try {
            parse_line(line);
        }
        catch (std::string s) {
            std::cerr << "Parse error";
            return 0;
        }
        return 1;
    }

    void parse_line(std::string line)
    {
        //std::cout << line << std::endl;
        commands.resize(0);
        commands.push_back("");

        bool in_quotes = false;
        for (auto i=0; i<line.length(); ++i) {
            if (line[i] == '\'' || line[i] == '\"') {
                in_quotes = !in_quotes;
            } else 
            if (!in_quotes && commands.back().length() > 0 && (is_space(line[i]) || 
               (is_control(line[i]) ^ is_control(line[i - 1])))) {
                commands.push_back("");
            } 
            if (in_quotes || !is_space(line[i])) {
                commands.back() += line[i];
            } //else {
             //   throw "Parse Error";
            //}
        }

        if (commands.back() == "")
            commands.pop_back();
        //for (auto i=0; i<commands.size(); ++i)
        //    std::cout << commands[i] << "+";
    }

    int process_commands(int start = 0)
    {
        std::vector<std::string> v_argv;
        int fd_in, fd_out;
        fd_in = fd_out = -1;

        int i;
        for (i = start; i<commands.size() && (!is_control(commands[i]) || is_redirect(commands[i])); ++i) {
            if (commands[i] == "<") {
                fd_in  = open(commands[i + 1].c_str(), O_RDONLY | O_CLOEXEC);
                ++i;
            } else if (commands[i] == ">") {
                fd_out = open(commands[i + 1].c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
                ++i;
            } else if (commands[i] == ">>") {
                fd_out = open(commands[i + 1].c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
                ++i;
            } else {
                v_argv.push_back(commands[i]);
            }
        }

        char** argv = (char**) malloc(sizeof(char*)*(v_argv.size()+1));
        for(auto i=0; i<v_argv.size(); ++i) 
            argv[i] = const_cast<char*>(v_argv[i].c_str());
        argv[v_argv.size()] = NULL;

        bool ispipe = (i < commands.size() && commands[i] == "|");
        int fd[2];

        if (ispipe)
            pipe(fd);

        if (fork() == 0) {
            if (ispipe) {
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }

            if (fd_in != -1) 
                dup2(fd_in, STDIN_FILENO);
            if (fd_out != -1) 
                dup2(fd_out, STDOUT_FILENO);

            execvp(argv[0], argv);
            throw "Exec error";
        }

        if (fd_in != -1) 
            close(fd_in);
        if (fd_out != -1) 
            close(fd_out);
       
        int status;
        //std::cout << commands[i] << std::endl;
        if (i < commands.size() && is_logic_cmd(commands[i])) {
            wait(&status);
            //std::cout << WEXITSTATUS(status) << std::endl;
            if ((commands[i] == "&&" && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) ||
                (commands[i] == "||" && WIFEXITED(status) && WEXITSTATUS(status) == 0)
               ) {
                if (i < commands.size())
                    ++i;
                while (i < commands.size() && !is_logic_cmd(commands[i]) && commands[i] != "|") 
                    ++i;
            }
        }

        if (i < commands.size() && fork() == 0) {
            if (ispipe) {
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO);
                close(fd[0]);
            }
            exit(process_commands(i + 1));
        }

        if (ispipe) {
            close(fd[0]);
            close(fd[1]);
        }

        while (wait(&status) != -1) {
            //sleep(0.01);
        }
        free(argv);
        return WEXITSTATUS(status);
    }

    bool execute()
    {
        int status;
       // std::cout << commands.back() << std::endl;
        if (commands.back() != "&") {
            if (fork() == 0) 
                exit(process_commands());
            wait(&status);
        } else {
            commands.pop_back();
            if (fork() == 0) {
                int pid;
                if ((pid = fork()) == 0) 
                    exit(process_commands());
                std::cerr << "Spawned child process " << pid << std::endl;
                wait(&status);
                int y = WEXITSTATUS(status);
                std::cerr << "Process " << pid << " exited: " << y << std::endl;
                exit(0);
            }
        }
        return WEXITSTATUS(status);
    }

};

bool getline(std::string& line) {
    line = "";
    int c = 0;
    while ((c = getchar()) > 0 && c != '\n') {
     //   std::cout << c << std::endl;
        line += char(c);
    }
    return c > 0;
}

int main(int argc, char** argv)
{
    Shell S;
    //std::string line;
    int retval = 0;
    char buf[100000];
    std::vector<std::string> lines(0);
    while( fgets(buf, sizeof(buf), stdin) ) {
        lines.push_back(std::string(buf));
        //std::cout << lines.back() << std::endl;
    }
    for(auto line : lines ) {
        //puts(buf);
       // std::cout << "XXX:" << line << std::endl;
        //line = std::string(buf);
        //std::cout << "XXX:" << line << std::endl;
        //puts(buf);
        S.set_cmd_line(line);
        retval = S.execute();
        //buf[0] = '\0';
        //std::getline(std::cin, line);
    } ////!std::cin.bad() && !std::cin.eof());
    return retval;
}