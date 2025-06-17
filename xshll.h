#define LIMIT 256
#define MAX_LINE 1024
#define MAX_ARGS 64

//INITIAL/BASIC FUCNTIONS
void welcomeScreen(); //Welcome screen banner for XSHLL
void showHelp(); //Command built-in for showing help for XSHLL
void shellPrompt(); //Average prompt of XSHLL

//ESSENTIAL FUNCTIONS
int changeDirectory(char *args[]); //chdir throws a return number

//MANIPULATING FUNCTIONS
int handleBuiltIn(char **args); //throws the code of failure or success
char** handlePipes(char *args[256]); //TODO:function to handle pipes in the user input
void executePipeline(char *[]); //If it's not a parsed command it means it's a launched
//program and hopefully, TODO:later on we can add it the & for running it from the background
