#include <iostream>
#include <string>
#include <cstdlib>
#include <sstream>
#include <unistd.h>
#include<vector>
#include<sys/wait.h>
#include <fcntl.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <dirent.h>
#include <iomanip>

void handle_cd_home();
void handle_cd_command(const std::string& path);
void handle_pwd_command();
void execute_external_command(const std::vector<std::string>& args, const std::string& redirect_file, const std::string& redirect_operator);
void handle_type_command(const std::string& command_to_check);
std::vector<std::string> parse_command_line(const std::string& command);
void execute_pipeline_command(const std::vector<std::string>& args);

static int history_saved_count=0;
void handle_history_comamnd(const std::vector<std::string>& args){
  int count= history_length;
  int limit= history_length;
  if (args.size()==3 && args[1]=="-a"){
    const std::string& filename=args[2];
    int num_to_append= history_length - history_saved_count;
    if (num_to_append>0){
      if (append_history(num_to_append, filename.c_str())!=0){
        std::cerr<< "shell: history: could not append to file: "<< filename<< std::endl;
      }
      else{
        history_saved_count=history_length;
      }
    }
    return;
  }
   if (args.size()>=3 && args[1]=="-w"){
    const std::string& filename= args[2];
    if (write_history(filename.c_str()) !=0){
      std::cerr<< "shell: history could not read history file: "<< filename<< std::endl;
    }
    return;
  }
  if (args.size()>=3 && args[1]=="-r"){
    const std::string& filename= args[2];
    if (read_history_range(filename.c_str(), -1, -1) !=0){
      std::cerr<< "shell: history could not read history file: "<< filename<< std::endl;
    }
    return;
  }
  if (args.size() >1){
    try{
      limit= std::stoi(args[1]);
    } catch (const std::exception& e){
      limit=history_length;
    }
  }
  int start_index_internal=0;
  if (limit< history_length){
    start_index_internal= history_length-limit;
  }

  int base= history_base;
  for (int i=start_index_internal; i< history_length; ++i){
    HIST_ENTRY* entry= history_get(base+i);

    if (entry != NULL){
      std::cout<< std::setw(5)<< (base +i)<<" "<<entry->line<<std::endl;
    }
  }
}

void execute_multi_pipeline(const std::vector<std::vector<std::string>>& commands){
  int num_commands= commands.size();
  std::vector<pid_t> pids;
  int input_fd= STDIN_FILENO;
  for (int i=0; i< num_commands; ++i){
    int pipefd[2];
    bool is_last= (i==num_commands-1);
    if (!is_last){
      if (pipe(pipefd)==-1){
        std::cerr<< "shell: pipe creation failed"<<std::endl;
        for (pid_t pid: pids) waitpid(pid, NULL, 0);
        return;
      }
    }
    pid_t pid= fork();
    if (pid==-1){
      std::cerr<< "shell: fork failed"<<std::endl;
    return;
    }
    if (pid==0){
      if (input_fd!= STDIN_FILENO){
        if (dup2(input_fd, STDIN_FILENO)==-1){
          exit(1);
        }
        close(input_fd);
      }
      if (!is_last){
        if (dup2(pipefd[1], STDOUT_FILENO)==-1){
          exit(1);
        }
        close(pipefd[1]);
        close(pipefd[0]);
      }
      execute_pipeline_command(commands[i]);
    }
    else{
      pids.push_back(pid);
      if(input_fd!=STDIN_FILENO){
        close(input_fd);
      }
      if (!is_last){
        close(pipefd[1]);
        input_fd=pipefd[0];
      }
    }
  }
  int status;
  for (pid_t pid: pids){
    waitpid(pid, &status, 0);
  }
}


void execute_pipeline_command(const std::vector<std::string>& args){
 const std::string& cmd=args[0];

  if (cmd=="exit"||cmd=="pwd"||cmd=="type"||cmd=="cd" || cmd=="history"){
    if (cmd=="exit"){
      exit(0);
    }
    else if (cmd=="echo"){
      if (args.size()>1){
        for (size_t i=1; i<args.size();++i){
          std::cout<< args[i]<<(i< args.size()-1 ? " ":"");
        }
      }
      std::cout<<std::endl;
      exit(0);
    }
    else if (cmd=="pwd"){
      handle_pwd_command();
      exit(0);
    }
    else if (cmd== "cd"){
      handle_cd_command(args.size() > 1 ? args[1]:""); exit(0);
    }
    else if(cmd=="type"){
      if (args.size()>1) handle_type_command(args[1]);
      exit(0);
    }

    else if (cmd=="history"){
      handle_history_comamnd(args);
      exit(0);
    }
  }
  else{
    std::vector<char*>c_args;
    for (const auto& arg: args) c_args.push_back(const_cast<char*>(arg.c_str()));
    c_args.push_back(nullptr);

    execvp(c_args[0], c_args.data());
    std::cerr<< args[0]<< ": command not found"<< std::endl;
    exit(1);
  }
}

void execute_pipeline(const std::vector<std::string>& arg1, const std::vector<std::string>& arg2){
  int pipefd[2];
  if (pipe(pipefd)==-1){
    std::cerr<< "shell: pipe failed"<<std::endl;
    return;
  }

  pid_t pid1=fork();
  if (pid1==0){
    close(pipefd[0]);

    if (dup2(pipefd[1],STDOUT_FILENO)==-1){
      std::cerr<< "shell: dup2 failed for pipe writer"<< std::endl;
      exit(1);
    }
    close(pipefd[1]);

    execute_pipeline_command(arg1);
  }else if (pid1<0){
    std::cerr<< "shell: fork failed for LHS"<<std::endl;
    return;
  }

  pid_t pid2=fork();
  if (pid2==0){
    close(pipefd[1]);

    if (dup2(pipefd[0],STDIN_FILENO)==-1){
      std::cerr<< "shell: dup2 failed for pipe writer"<< std::endl;
      exit(1);
    }
    close(pipefd[0]);

    execute_pipeline_command(arg2);
  }
  else if (pid2<0){
    std::cerr<< "shell: fork failed for LHS"<<std::endl;
    return;
  }

  close(pipefd[0]);
  close(pipefd[1]);
  int status;
  waitpid(pid1, &status, 0);
  waitpid(pid2, &status, 0);
}

const char* BUILTINS[]={
    "echo", "exit","pwd","cd","type","history", NULL
  };

char *unified_generator(const char *text, int state){
  static std::vector<std::string> matches;
  static int match_index;
  static int builtin_idx;
  static int len;
  static bool searching_builtins;

  if(!state){
    matches.clear();
    match_index=0;
    searching_builtins=true;
    len=strlen(text);
    builtin_idx=0;

    const char* path_cstr=std::getenv("PATH");
    if(path_cstr){
      std::stringstream ss(path_cstr);
      std::string dir_path;

      while(std::getline(ss, dir_path, ':')){
        DIR* dir=opendir(dir_path.c_str());
        if(!dir) continue;

        struct dirent* entry;

        while((entry=readdir(dir))!=NULL){
          const char* filename=entry->d_name;
          
          if (strncmp(filename, text, len)==0){
            std::string full_path= dir_path +"/"+filename;
            if (access(full_path.c_str(), X_OK)==0){
              matches.push_back(filename);
            }
          }
        }
        closedir(dir);
      }
    }
  }
  
  if (searching_builtins){
    while (BUILTINS[builtin_idx]){
      const char *name= BUILTINS[builtin_idx++];
      if  (strncmp(name, text, len)==0){
          return strdup(name);
      }
    }
    searching_builtins=false;
  }


  if (match_index< matches.size()){
    return strdup(matches[match_index++].c_str());
  }
  return NULL;
}


char **command_completion(const char *text, int start, int end){

  rl_completion_append_character= ' ';

  return rl_completion_matches(text, unified_generator);
}

void handle_cd_home(){
  const char* home_cstr=std::getenv("HOME");
  if (home_cstr==NULL){
    std::cerr<< "cd: HOME environment variable not set."<< std::endl;
    return;
  }

  if(chdir(home_cstr)!=0){
    std::cerr<<"cd: "<< home_cstr <<": Cannot change to home directory."<< std::endl;
  }
}

void handle_cd_command(const std::string& path){

  
  if(chdir(path.c_str()) !=0){
    std::cerr<<"cd: "<< path<< ": No such file or directory"<< std::endl;
  }
}


void handle_pwd_command(){
  char* cwd_cstr=getcwd(NULL, 0);
  if (cwd_cstr==NULL){
    std::cerr<< "pwd: error retrieving current directory."<<std::endl;
  }
  else{
    std::cout<< cwd_cstr<<std::endl;
    free(cwd_cstr);
  }
}


std::vector<std::string> parse_command_line(const std::string& command){
  std::vector<std::string> args;
  std::string current_arg;
  bool in_single_quotes=false;
  bool in_double_quotes= false;
  bool is_escaped= false;

  for (size_t i=0; i<command.length();++i){
    char c= command[i];
    if (is_escaped){
      is_escaped=false;
      current_arg+=c;
      continue;
    }
    if (c=='\\'){
       bool should_escape = false;
        

        if (in_double_quotes && !in_single_quotes) { 
            if (i+1 < command.length()){
              char next_c = command[i+1];

              if (next_c == '"' || next_c == '\\'){ 
                should_escape = true;
              }
            }
        }
        

        else if (!in_single_quotes && !in_double_quotes) {
            should_escape = true;
        }
        

        if (should_escape) {
            is_escaped = true;
            continue; 
        }
    }

    if(c== '\''){
      if (!in_double_quotes){
        in_single_quotes=!in_single_quotes;
        continue;
      }

    }
    if (c=='"'){
      if(!in_single_quotes){
      in_double_quotes=!in_double_quotes;
      continue;
    }}

    if(isspace(c) && !in_single_quotes && !in_double_quotes){
      if(!current_arg.empty()){
        args.push_back(current_arg);
        current_arg.clear();
      }
      continue;
    }
    current_arg+=c;
  }
    
  
  if (!current_arg.empty()){
    args.push_back(current_arg);
  }
  if (in_single_quotes || in_double_quotes || is_escaped){
    std::cerr<<"shell: unclosed quote"<<std::endl;
    args.clear();
  }
  return args;
}


void execute_external_command( const std::vector<std::string>& args, const std::string& redirect_file, const std::string& redirect_operator){
  std::vector<char*> c_args;
  std::vector<std::string> arg_storage;  

for (const auto& arg : args) {
    arg_storage.push_back(arg);  
}

for (auto& s : arg_storage) {
    c_args.push_back(const_cast<char*>(s.c_str()));
}

c_args.push_back(nullptr);

  pid_t pid= fork();
  if(pid==0){
    if (!redirect_file.empty()){
        int target_fd=-1;
        if (redirect_operator=="2>"||redirect_operator=="2>>"){
          target_fd=STDERR_FILENO;
        }
     
        else{ target_fd= STDOUT_FILENO;}
        int flags= O_WRONLY | O_CREAT;
        if (redirect_operator==">>"|| redirect_operator=="1>>"||redirect_operator=="2>>"){
          flags |= O_APPEND;
        }
        else{
          flags|=O_TRUNC;
        }
      int fd= open(redirect_file.c_str(), flags, 0644);
      if (fd==-1){
        std::cerr<< "shell: failed to open redirect file: "<< redirect_file<<std::endl;
        exit(1);
      }

      if (dup2(fd, target_fd)==-1){
        std::cerr<< "shell: failed to redirect output"<<std::endl;
        exit(1);
      }
      close(fd);
    }    
    execvp(c_args[0],c_args.data());
    std::cerr << args[0]<< ": command not found"<< std::endl;
    exit(1);
  }

  else if (pid>0){
    int status;
    waitpid(pid, &status, 0);
  }
  else{
    std::cerr<<"shell: fork failed"<<std::endl;
  }
}

void handle_type_command(const std::string& command_to_check){
  if (command_to_check=="echo" || command_to_check=="exit"|| command_to_check=="type" || command_to_check=="pwd"|| 
    command_to_check=="cd" || command_to_check=="history"){
    std::cout<< command_to_check<<" is a shell builtin"<< std::endl;
    return;
}

  const char* path_cstr= std::getenv("PATH");
  if (!path_cstr){
    std::cout << command_to_check << ": not found"<<std::endl;
    return;
  }

  std::string path_env(path_cstr);
  std::stringstream path_stream(path_env);
  std::string directory;
  bool found_executable=false;

  while(std::getline(path_stream, directory, ':')){

    std::string full_path= directory+ "/"+command_to_check;

    if (access(full_path.c_str(), X_OK)==0){
      std::cout<< command_to_check << " is "<< full_path<<std::endl;
      found_executable= true;
      break;
    }
  }
  if (!found_executable){
    std::cout<< command_to_check << ": not found"<< std::endl;
  }
  
}
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  rl_attempted_completion_function=command_completion;
  const char* histfile_path= std::getenv("HISTFILE");
  if (histfile_path!=NULL){
    if (read_history(histfile_path)!=0){
      // std::cerr<< "shell: warning: could not read history file: "<< histfile_path<< std::endl;
    }}
while (true){
 
   char *line= readline("$ ");
    if(!line) break;
    std::string command(line);
    if (!command.empty()){
      add_history(line);
      free(line);
    }
    else{
      free(line);
      continue;
    }
  if (command=="exit 0" || command=="exit"){
    const char* histfile_path= std::getenv("HISTFILE");
    if (histfile_path!= NULL){
      if (write_history(histfile_path)!=0){}}
    return 0;}
  else if (command=="pwd"){
      handle_pwd_command();
    }
  else if (command=="cd ~"||command=="cd"){
    handle_cd_home();
  }
  else if (command.find("cd ")==0){
    std::string path_arg=command.substr(3);
    handle_cd_command(path_arg);
  }

  else if (command.find("history")==0){
    std::vector<std::string> args= parse_command_line(command);
    handle_history_comamnd(args);
  }
  else if (command.find("echo") ==0 ) {
    std::vector<std::string> full_args= parse_command_line(command);
    auto pipe_it=std::find(full_args.begin(),full_args.end(), "|");
    if (pipe_it!=full_args.end()){
      std::vector<std::string> arg1(full_args.begin(), pipe_it);
      std::vector<std::string> arg2(pipe_it+1, full_args.end());
      if(!arg1.empty() && !arg2.empty()){
          execute_pipeline(arg1,arg2);
      }
      else{
        std::cerr<< "shell: invalid pipeline syntax (builtin)"<<std::endl;
      }
    }

  else{
    std::string redirect_file;
    std::string redirect_operator;
    for (size_t i=0; i< full_args.size();++i){
      if (full_args[i]==">"|| full_args[i]=="1>"|| full_args[i]=="2>"||full_args[i]==">>"||full_args[i]=="1>>"||full_args[i]=="2>>"){
        if (i+1<full_args.size()){
          redirect_operator=full_args[i];
          redirect_file=full_args[i+1];
          full_args.erase(full_args.begin()+i, full_args.begin()+i+2);
          break;
        }
      }
    }
    int origina_stdout=-1;
    int target_fd=STDOUT_FILENO;
    int fd=-1;
    if(!redirect_file.empty()){

      if (redirect_operator=="2>"||redirect_operator=="2>>"){
        target_fd=STDERR_FILENO;
      }
      else{
        target_fd=STDOUT_FILENO;
      }
        int flags= O_WRONLY | O_CREAT;
        if (redirect_operator==">>"|| redirect_operator=="1>>"){
          flags |= O_APPEND;
        }
        else{
          flags|=O_TRUNC;
        }
      origina_stdout=dup(target_fd);
      fd= open(redirect_file.c_str(), flags,0644);
      if (fd==-1){
        std::cerr<<"shell: failed to open redirect file: "<< redirect_file<< std::endl;
      }
      else{
        dup2(fd, target_fd);
        close(fd);
      }
    }
    if (full_args.size()>1){
      for (size_t i=1; i<full_args.size(); ++i){
        std::cout << full_args[i];
        if (i< full_args.size()-1){
          std::cout<< " ";
        }
      }
    }
    std::cout<<std::endl;
    if(origina_stdout!=-1){
      dup2(origina_stdout, target_fd);
      close(origina_stdout);
    }}
  }
  
  else if (command.find("type")==0){
    if (command.length()>5 &&  command.at(4)==' '){
      std::string command_to_check= command.substr(5);
      handle_type_command(command_to_check);
    }
    else if (command=="type"){
      handle_type_command("type");
    }
    else {
      std::cout << command << ": command not found" << std::endl;
    }
    }
    
     else{
    std::vector<std::string> full_args=parse_command_line(command);
    if (full_args.empty()) continue;
    std::vector<std::vector<std::string>> commands;
    auto current_start= full_args.begin();
    auto end_it= full_args.end();
    while (current_start!=end_it){
      auto pipe_it= std::find(current_start, end_it, "|");
      std::vector<std::string> segment_args(current_start, pipe_it);
      if (!segment_args.empty()){
        commands.push_back(segment_args);
      }
      if (pipe_it== end_it) break;
      current_start=pipe_it+1;
    }
    if (commands.size()>1){
      execute_multi_pipeline(commands);
    }


    else{


      std::string redirect_file;
      std::string redirect_operator;
      for(size_t i=0; i<full_args.size(); ++i){
        if (full_args[i]==">"|| full_args[i]=="1>"|| full_args[i]=="2>"|| full_args[i]==">>"|| full_args[i]=="1>>"||full_args[i]=="2>>"){
          if (i+1<full_args.size()){
            redirect_operator=full_args[i];
            redirect_file= full_args[i+1];
            full_args.erase(full_args.begin()+i, full_args.begin()+i+2);
            break;
          }
        }


      }
      if (!full_args.empty()){
        execute_external_command(full_args, redirect_file,redirect_operator);
      }
    }}
  }
  
 
 return 0;
}



