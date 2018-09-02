#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <sstream>
#include "ProgramOptions.hxx"

static inline bool stringContains(const std::string& container, const std::string& toBeContained);
static std::pair<unsigned int,std::vector<std::string>> parseOptions(int argc, char** argv);
static std::deque<std::string> parseInput();
static void preProcessFile(const std::string& filename, std::deque<std::string>& definitions);
static std::string invokeCBMC(const std::vector<std::string>& filenames, unsigned int unwindCount);
static std::string trimOutput(const std::string& verboseOutput);
static inline void ReplaceAll(std::string& str, const std::string& from, const std::string& to);

int main( int argc, char** argv )
{
    auto filesAndUnwinds = parseOptions(argc,argv);
    auto& files = filesAndUnwinds.second;
    auto& unwinds = filesAndUnwinds.first;

    if(files.empty())
        return 0;

    auto inputDefinitions = parseInput();

    for(const auto& file : files)
        preProcessFile(file,inputDefinitions);

    if(!inputDefinitions.empty())
    {
        std::cerr<<"didn't bind all the input parameters, error!"<<std::endl;
        return -1;
    }

    auto output = invokeCBMC(files,unwinds);

    std::cout << trimOutput(output);

    return 0;
}

static std::pair<unsigned int,std::vector<std::string>> parseOptions(int argc, char** argv)
{
    po::parser options;

    options["help"]
        .abbreviation('?')
        .description("prints this help screen")
        .callback([&](){std::cout<<options<<std::endl;});
    
    options["unwind"]
        .abbreviation('u')
        .type(po::u32)
        .description("specifies the number of loop-unwinds cbmc should perform")
        .fallback(100);

    options[""]
        .type(po::string)
        .multi();

    if(!options(argc,argv)) {
        std::cerr<<"error parsing program options"<<std::endl;
        return std::make_pair(0,std::vector<std::string>());
    }

    if(options["help"].size())
        return std::make_pair(0,std::vector<std::string>());

    std::vector<std::string> files;

    for(auto&& file : options[""])
        files.push_back(file.string);
    
    if(files.empty())
        std::cerr << "please specify at least one source file"<<std::endl;

    return std::make_pair(options["unwind"].get().u32,std::move(files));
}

static std::deque<std::string> parseInput()
{
    std::deque<std::string> toReturn;
    while(!std::cin.eof())
    {
        std::string buf;
        std::getline(std::cin,buf);
        if(buf.size()>0 && buf[buf.size()-1]=='\r')
            buf.erase(buf.size()-1);
        toReturn.push_back(std::move(buf));
    }
    return std::move(toReturn);
}

static void preProcessFile(const std::string& filename, std::deque<std::string>& definitions)
{
    std::ifstream inputStream(filename);
    if(!inputStream)
    {
        std::cerr<<"failed to open "<<filename<<std::endl;
        return;
    }

    std::ofstream outputStream(filename+".cbmc.c");
    if(!outputStream)
    {
        std::cerr<<"failed to open "<<filename<<".cbmc.c for writing"<<std::endl;
        return;
    }

    outputStream<<"#include <stdio.h>"<<std::endl;
    outputStream<<"#include <assert.h>"<<std::endl;

    std::string lineBuffer;
    while(std::getline(inputStream,lineBuffer) &&
         lineBuffer.size()>=1 &&
          lineBuffer[0]!='#' &&
           lineBuffer.find("f()")==std::string::npos)
    {
        outputStream<<"const "<<lineBuffer<<" = "<<definitions.front()<<";"<<std::endl;
        if(stringContains(lineBuffer,"[]"))
        {
            auto rightBeforeFirstBracket = lineBuffer.find("[");
            auto firstWhitespaceToLeftOfBracket = lineBuffer.find_last_of(" ",rightBeforeFirstBracket)+1;
            auto varName = lineBuffer.substr(firstWhitespaceToLeftOfBracket,rightBeforeFirstBracket-firstWhitespaceToLeftOfBracket);
            outputStream<<"const int "<< varName << "l = sizeof("<< varName << ")/sizeof("<<varName<<"[0]);"<<std::endl;
        }
        definitions.pop_front();
    }

    while(!inputStream.eof())
    {
        // replace p( with printf(
        ReplaceAll(lineBuffer,"p(\"","printf(\"\\n");
        // replace a( with assert(
        ReplaceAll(lineBuffer,"a(","assert(");

        if(lineBuffer=="f()" || lineBuffer=="f()\r")
            outputStream<<"int main() {";
        else
            outputStream<<lineBuffer<<std::endl;

        std::getline(inputStream,lineBuffer);
    }

    outputStream<<"}";
}

static inline std::string buildOutPutFileName(const std::vector<std::string>& filenames)
{
    std::string outPutFile = "";
    for(const auto& file : filenames)
        outPutFile += file + "_";
    outPutFile += "out";
    return std::move(outPutFile);
}

static std::string invokeCBMC(const std::vector<std::string>& filenames, unsigned int unwindCount) 
{
    std::string command = "cbmc --beautify --trace --unwind ";
    command += std::to_string(unwindCount);
    for(const auto& file : filenames)
        command += " " + file + ".cbmc.c";
    auto outPutFile = buildOutPutFileName(filenames);
    command += " > " + outPutFile;
    command += " 2> /dev/null";

    std::system(command.c_str());

    std::ifstream cbmcOutput(outPutFile);
    if(!cbmcOutput)
    {
        std::cerr<<"failed to read the CBMC output"<<std::endl;
        return "";
    }
    std::stringstream sstr;
    sstr << cbmcOutput.rdbuf();
    return std::move(sstr.str());
}

static inline bool stringContains(const std::string& container, const std::string& toBeContained)
{
    return container.find(toBeContained)!=std::string::npos;
}

const std::string removedLinesSubStrings[] = {
    "CBMC version",
    "Parsing ",
    "Unwinding loop ",
    "Not unwinding loop ",
    "size of program expression: ",
    "simple slicing removed ",
    "Generated ",
    "Passing problem to propositional reduction",
    "converting SSA",  
    "Running propositional reduction",
    "Post-processing",
    "Solving with MiniSAT",
    "variables",
    "SAT checker: instance is ",
    "Runtime decision procedure:",
    " assertion ",
    "** Results",
    "Trace for ",
    "**",
    "VERIFICATION FAILED",
    "VERIFICATION SUCCESSFUL",
    "file <command-line> ",
    "<built-in>:",
    "Converting",
    "Type-checking ",
    "Generating GOTO Program",
    "Adding CPROVER library",
    "Removal of function pointers and virtual functions",
    "Partial Inlining",
    "Generic Property Instrumentation",
    "Starting Bounded Model Checking"
};

static std::string trimOutput(const std::string& verboseOutput)
{
    std::string outputBuffer;
    std::string lineBuffer;
    std::istringstream stream(verboseOutput);

    while(std::getline(stream,lineBuffer))
    {
        if(std::any_of(std::begin(removedLinesSubStrings),
            std::end(removedLinesSubStrings),
            [&lineBuffer](const std::string& bad){return stringContains(lineBuffer,bad);}))
            continue;
        if(stringContains(lineBuffer,"State ") || stringContains(lineBuffer,"Violated property:"))
        {
            while(lineBuffer!="" && lineBuffer!="\r" && !stream.eof())
                std::getline(stream,lineBuffer);
               
            continue;
        }
        if(lineBuffer=="" || lineBuffer=="\r")
            continue;

        outputBuffer+=lineBuffer;
        outputBuffer+="\n";
    }

    return std::move(outputBuffer);
}

// from stackoverflow.com
// question: https://stackoverflow.com/q/2896600
// asker: https://stackoverflow.com/users/194510/big-z
// answer: https://stackoverflow.com/a/24315631 
// answerer: https://stackoverflow.com/users/1715716/gauthier-boaglio
static inline void ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != std::string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}