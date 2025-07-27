#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <stack>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <cstdint>
#include <algorithm>

using namespace std;

inline int64_t keyRC(int row, int col) {
    return (static_cast<int64_t>(row) << 32) | (uint32_t)col;
}

class Interpreter {
private:
    unordered_map<int64_t, int> tape;  // Memory grid
    int row = 0, col = 0;

    vector<char> program;
    unordered_map<int, int> jumpTable;
    
    vector<unsigned char> inputBuffer;
    int inputPtr = 0;

    map<string, string> macros;
    unordered_map<string, pair<int,int>> labels;

    unordered_map<string, vector<int>> vectors;
    unordered_map<string, unordered_map<int64_t, int>> snapshots;

public:
    void loadProgram(const string& filename) {
        ifstream fin(filename);
        if (!fin) {
            cerr << "Could not open file: " << filename << endl;
            exit(1);
        }
        string code((istreambuf_iterator<char>(fin)), istreambuf_iterator<char>());
        fin.close();
        
        // Parse macros
        string expandedCode;
        for (size_t i = 0; i < code.size(); i++) {
            if (code[i] == '(') {
                size_t closeParen = code.find(')', i);
                size_t eqBrace = code.find("={", closeParen);
                size_t closeBrace = code.find('}', eqBrace);
                if (closeParen != string::npos && eqBrace != string::npos && closeBrace != string::npos) {
                    string name = code.substr(i + 1, closeParen - i - 1);
                    string body = code.substr(eqBrace + 2, closeBrace - (eqBrace + 2));
                    macros[name] = body;
                    i = closeBrace;
                    continue;
                }
            }
            expandedCode.push_back(code[i]);
        }

        // Expand macros (up to 50 passes)
        bool replaced = true;
        int pass = 0;
        while (replaced && pass < 50) {
            replaced = false;
            for (auto &entry : macros) {
                size_t pos = 0;
                while ((pos = expandedCode.find(entry.first, pos)) != string::npos) {
                    expandedCode.replace(pos, entry.first.size(), entry.second);
                    pos += entry.second.size();
                    replaced = true;
                }
            }
            pass++;
        }
        if (pass == 50) {
            cerr << "Macro expansion limit reached (possible recursion)." << endl;
            exit(1);
        }
        
        // Strip comments & whitespace
        string cleaned;
        for (size_t i = 0; i < expandedCode.size(); i++) {
            if (expandedCode[i] == '`' && i + 2 < expandedCode.size() && expandedCode[i+1] == '`' && expandedCode[i+2] == '`') {
                i += 3;
                while (i + 2 < expandedCode.size() && !(expandedCode[i] == '`' && expandedCode[i+1] == '`' && expandedCode[i+2] == '`')) i++;
                i += 2;
                continue;
            }
            if (!isspace((unsigned char)expandedCode[i]))
                cleaned.push_back(expandedCode[i]);
        }
        for (char c : cleaned) program.push_back(c);
        
        buildJumpTable();
    }

    void run() {
        srand((unsigned)time(nullptr));
        for (int pc = 0; pc < (int)program.size(); ++pc) {
            char cmd = program[pc];

            // Handle normal commands
            switch(cmd) {
                case '>': col++; break;
                case '<': col--; break;
                case '^': row--; break;
                case 'v': row++; break;
                case '+': setCell(getCell() + 1); break;
                case '-': setCell(getCell() - 1); break;

                case '|': incrementColumn(); break;
                case '_': incrementRow(); break;

                case '~': setCell(rand() & 0xFF); break;

                case '?':
                    if (pc + 1 < (int)program.size() && program[pc+1] == '?') {
                        for (unsigned char b : inputBuffer) {
                            setCell(b);
                            col++;
                        }
                        pc++;
                    } else handleInput();
                    break;
                
                case '.':
                    if (pc + 1 < (int)program.size() && program[pc+1] == '(') {
                        int count = 0; pc += 2;
                        while (program[pc] != ')') {
                            if (program[pc] == '+') count++;
                            else if (program[pc] == '-') count--;
                            pc++;
                        }
                        for (int i = 0; i <= count; i++) {
                            cout << (char)getCell();
                            col++;
                        }
                    } else cout << (char)getCell();
                    break;
                
                case '*':
                case '/':
                    assert(pc + 1 < (int)program.size() && program[pc+1] == '(');
                    {
                        int count = 0; pc += 2;
                        while (program[pc] != ')') {
                            if (program[pc] == '+') count++;
                            else if (program[pc] == '-') count--;
                            pc++;
                        }
                        if (cmd == '*') setCell(getCell() * count);
                        else setCell(count == 0 ? getCell() : getCell() / count);
                    }
                    break;
                
                case '[': if (getCell() == 0) pc = jumpTable[pc]; break;
                case ']': if (getCell() != 0) pc = jumpTable[pc]; break;

                case 'l':
                    if (matchKeyword(pc, "label{")) {
                        pc += 6; string name;
                        while (program[pc] != '}') name.push_back(program[pc++]);
                        labels[name] = {row, col};
                    }
                    break;

                case '\\': // Extended commands
                    if (handleExtendedCommand(pc)) break;
                    break;

                case '!':
                    if (matchKeyword(pc, "!guide!")) {
                        printGuide(); pc += 6;
                    }
                    break;

                default: break;
            }
        }
    }

    /** Extended commands handler **/
    bool handleExtendedCommand(int &pc) {
        // Teleport
        if (matchKeyword(pc, "\\tp{")) {
            pc += 4; string name;
            while (program[pc] != '}') name.push_back(program[pc++]);
            if (labels.count(name)) { row = labels[name].first; col = labels[name].second; }
            return true;
        } 
        if (matchKeyword(pc, "\\tp(")) {
            pc += 4; string xStr, yStr;
            while (program[pc] != ',') xStr.push_back(program[pc++]); pc++;
            while (program[pc] != ')') yStr.push_back(program[pc++]);
            col += stoi(xStr); row -= stoi(yStr); 
            return true;
        }

        // Clone
        if (matchKeyword(pc, "\\clone{")) {
            pc += 7; string name;
            while (program[pc] != '}') name.push_back(program[pc++]);
            cloneCell(name); return true;
        }

        // Swap
        if (matchKeyword(pc, "\\swap{")) {
            pc += 6; string name;
            while (program[pc] != '}') name.push_back(program[pc++]);
            swapCell(name); return true;
        }

        // Save & Restore
        if (matchKeyword(pc, "\\save{")) {
            pc += 6; string name;
            while (program[pc] != '}') name.push_back(program[pc++]);
            saveSnapshot(name); return true;
        }
        if (matchKeyword(pc, "\\restore{")) {
            pc += 9; string name;
            while (program[pc] != '}') name.push_back(program[pc++]);
            restoreSnapshot(name); return true;
        }

        // Fill rectangle
        if (matchKeyword(pc, "\\fill{")) {
            pc += 6; string src, n1, n2;
            while (program[pc] != ',') src.push_back(program[pc++]); pc++;
            while (program[pc] != ',') n1.push_back(program[pc++]); pc++;
            while (program[pc] != '}') n2.push_back(program[pc++]);
            fillRectangle(src, n1, n2); return true;
        }

        // Math
        if (matchKeyword(pc, "\\math{")) {
            pc += 6; string name, valStr; char op;
            while (program[pc] != ',') name.push_back(program[pc++]); pc++;
            op = program[pc++]; pc++;
            while (program[pc] != '}') valStr.push_back(program[pc++]);
            mathOp(name, op, stoi(valStr)); return true;
        }

        // Vector Set
        if (matchKeyword(pc, "\\vset{")) {
            pc += 6; string name;
            while (program[pc] != '}') name.push_back(program[pc++]);
            vset(name); return true;
        }

        // PushBack
        if (matchKeyword(pc, "\\pb(")) {
            pc += 4; string vec, cell;
            while (program[pc] != ')') vec.push_back(program[pc++]); pc++;
            while (program[pc] != '}') cell.push_back(program[pc++]);
            pushBack(vec, cell); return true;
        }
                
        if (matchKeyword(pc, "\\mod{")) {
            pc += 6; 
            string name, valStr;
            while (program[pc] != ',') name.push_back(program[pc++]); pc++;
            while (program[pc] != '}') valStr.push_back(program[pc++]);

            if (labels.count(name)) {
                auto key = keyRC(labels[name].first, labels[name].second);
                int current = tape.count(key) ? tape[key] : 0;
                int divisor = stoi(valStr);
                if (divisor != 0) {
                    tape[key] = current % divisor;
                }
            }
            return true;
        }

        return false;
    }

    /** Extended functions **/

    void cloneCell(const string& name) {
        if (labels.count(name)) tape[keyRC(labels[name].first, labels[name].second)] = getCell();
    }

    void swapCell(const string& name) {
        if (labels.count(name)) {
            auto key = keyRC(labels[name].first, labels[name].second);
            int temp = tape.count(key) ? tape[key] : 0;
            tape[key] = getCell();
            setCell(temp);
        }
    }

    void saveSnapshot(const string& name) {
        snapshots[name] = tape; 
    }

    void restoreSnapshot(const string& name) {
        if (snapshots.count(name)) tape = snapshots[name];
    }

    void fillRectangle(const string& src, const string& name1, const string& name2) {
        if (!labels.count(src) || !labels.count(name1) || !labels.count(name2)) return;
        int val = tape[keyRC(labels[src].first, labels[src].second)];
        int r1 = min(labels[name1].first, labels[name2].first);
        int r2 = max(labels[name1].first, labels[name2].first);
        int c1 = min(labels[name1].second, labels[name2].second);
        int c2 = max(labels[name1].second, labels[name2].second);
        for (int r = r1; r <= r2; ++r)
            for (int c = c1; c <= c2; ++c)
                tape[keyRC(r, c)] = val;
    }

    void mathOp(const string& name, char op, int val) {
        if (!labels.count(name)) return;
        auto key = keyRC(labels[name].first, labels[name].second);
        int current = tape.count(key) ? tape[key] : 0;
        switch(op) {
            case '+': current += val; break;
            case '-': current -= val; break;
            case '*': current *= val; break;
            case '/': current = (val != 0) ? current / val : current; break;
        }
        tape[key] = current & 0xFF;
    }

    void vset(const string& name) {
        vectors[name] = vector<int>();
    }

    void pushBack(const string& vecName, const string& cellName) {
        if (!vectors.count(vecName) || !labels.count(cellName)) return;
        auto key = keyRC(labels[cellName].first, labels[cellName].second);
        vectors[vecName].push_back(tape.count(key) ? tape[key] : 0);
    }

private:
    int getCell() {
        int64_t key = keyRC(row, col);
        return tape.count(key) ? tape[key] : 0;
    }
    void setCell(int val) {
        tape[keyRC(row, col)] = val & 0xFF;
    }

    void incrementColumn() {
        for (auto &cell : tape) {
            int cellCol = (int)(cell.first & 0xFFFFFFFF);
            if (cellCol == col) cell.second = (cell.second + 1) & 0xFF;
        }
    }
    void incrementRow() {
        for (auto &cell : tape) {
            int cellRow = (int)(cell.first >> 32);
            if (cellRow == row) cell.second = (cell.second + 1) & 0xFF;
        }
    }

    void buildJumpTable() {
        stack<int> s;
        for (int i = 0; i < (int)program.size(); ++i) {
            if (program[i] == '[') s.push(i);
            else if (program[i] == ']') {
                if (s.empty()) { cerr << "Unmatched ']' at " << i << endl; exit(1); }
                int open = s.top(); s.pop();
                jumpTable[open] = i;
                jumpTable[i] = open;
            }
        }
        if (!s.empty()) { cerr << "Unmatched '[' at " << s.top() << endl; exit(1); }
    }

    void handleInput() {
        // If buffer is empty, actually read from stdin
        if (inputBuffer.empty()) {
            string token;
            if (!(cin >> token)) {
                // If there's no input, set 0 and bail
                setCell(0);
                return;
            }
            // Push all characters into buffer
            for (unsigned char c : token)
                inputBuffer.push_back(c);
            inputBuffer.push_back('\n'); // Sentinel or separator
        }

        // Feed one character at a time
        setCell(inputBuffer[inputPtr++]);
        if (inputPtr >= (int)inputBuffer.size()) {
            inputBuffer.clear();
            inputPtr = 0;
        }
    }

    bool matchKeyword(int idx, const string &kw) {
        for (int i = 0; i < (int)kw.size(); i++)
            if (idx + i >= (int)program.size() || program[idx + i] != kw[i]) return false;
        return true;
    }

    void printGuide() {
        cout << "=== SKIVE GUIDE ===\n"
            << "Pointer & Tape:\n"
            << "  > < ^ v           : Move pointer (right, left, up, down)\n"
            << "  + -               : Increment/decrement current cell (0-255)\n"
            << "  | _               : Increment entire column / entire row\n"
            << "  ~                 : Randomize current cell (0-255)\n"
            << "\n"
            << "I/O:\n"
            << "  ?                 : Read 1 byte into current cell\n"
            << "  ??                : Read full input stream sequentially into tape\n"
            << "  .                 : Print current cell as char\n"
            << "  .(+++++)          : Print current + N cells as chars (N = count of + or -)\n"
            << "\n"
            << "Loops & Macros:\n"
            << "  [ ]               : Loop while current cell != 0\n"
            << "  (name)={...}      : Define macro\n"
            << "  (name)            : Expand macro\n"
            << "\n"
            << "Labels & Teleport:\n"
            << "  label{name}       : Create label at current pointer location\n"
            << "  \\\\tp{name}         : Teleport to label\n"
            << "  \\\\tp(x,y)          : Relative teleport by (x,y) from current pointer\n"
            << "\n"
            << "Math:\n"
            << "  *(+++)            : Multiply current cell by N (# of + or -)\n"
            << "  /(---)            : Divide current cell by N (# of + or -)\n"
            << "  \\\\math{label,op,val}: Apply op (+,-,*,/) with val to labeled cell\n"
            << "  \\\\mod{label,val}   : Set labeled cell to (cell % val)\n"
            << "\n"
            << "Snapshots & Cloning:\n"
            << "  \\\\clone{label}     : Clone current cell value into labeled cell\n"
            << "  \\\\swap{label}      : Swap current cell with labeled cell\n"
            << "  \\\\save{name}       : Snapshot entire tape as 'name'\n"
            << "  \\\\restore{name}    : Restore tape snapshot 'name'\n"
            << "\n"
            << "Region & Vectors:\n"
            << "  \\\\fill{src,n1,n2}  : Fill rectangle between labels n1 & n2 with src cell's value\n"
            << "  \\\\vset{name}       : Create empty vector named 'name'\n"
            << "  \\\\pb(vec,label)    : Push the labeled cell's value into vector 'vec'\n"
            << "\n"
            << "Comments:\n"
            << "  ``` ... ```        : Multiline comment (ignored)\n"
            << "\n"
            << "Meta:\n"
            << "  !guide!            : Show this guide\n"
            << "===================\n";
    }
};

int main() {
    Interpreter i;
    i.loadProgram("program.txt");
    i.run();
    return 0;
}
