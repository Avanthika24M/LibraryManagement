// Library Management System - main.cpp
// C++20 implementation; no external database or testing dependencies.

#include "library.hpp"
#include <iostream>
#include <limits>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cctype>
#include <climits>
using namespace lms;
namespace {
    // Read one line of user input from the console.
    std::string ask(const std::string& label) {
        std::cout << label;
        std::string s;
        std::getline(std::cin,s);
        return s;
    }
    // Read passwords without displaying their characters; show one * per character.
    // Mask password input with asterisks in an interactive Linux terminal.
    std::string askPassword(const std::string& label) {
        std::cout << label<<std::flush;
        if(!isatty(STDIN_FILENO)) {
            std::string password;
            std::getline(std::cin,password);
            return password;
        }
        termios original {
        };
        if(tcgetattr(STDIN_FILENO,&original)!=0) {
            std::string password;
            std::getline(std::cin,password);
            return password;
        }
        termios hidden=original;
        hidden.c_lflag &= static_cast<tcflag_t>(~(ECHO|ICANON));
        if(tcsetattr(STDIN_FILENO,TCSANOW,&hidden)!=0) {
            std::string password;
            std::getline(std::cin,password);
            return password;
        }
        std::string password;
        char ch=0;
        while(read(STDIN_FILENO,&ch,1)==1) {
            if(ch=='\n'||ch=='\r')break;
            if(ch==127||ch=='\b') {
                if(!password.empty()) {
                    password.pop_back();
                    std::cout << "\b \b"<<std::flush;
                }
            }
            else if(ch==3) {
                password.clear();
                break;
            }
            else if(static_cast<unsigned char>(ch)>=32 && password.size()<256) {
                password.push_back(ch);
                std::cout << '*'<<std::flush;
            }
        }
        tcsetattr(STDIN_FILENO,TCSANOW,&original);
        std::cout << '\n';
        return password;
    }
    // Keep missing, nonnumeric, and out-of-range values distinguishable.
    bool number(const std::string& label, int& result) {
        const std::string raw=ask(label);
        const std::string field=label.substr(0,label.find(':'));
        if(raw.empty()) { std::cout << "Error: " << field << " is not entered.\n"; return false; }
        size_t first=raw.find_first_not_of(" \t");
        if(first==std::string::npos) { std::cout << "Error: " << field << " is not entered.\n"; return false; }
        size_t last=raw.find_last_not_of(" \t");
        std::string value=raw.substr(first,last-first+1);
        size_t start=(value[0]=='+'||value[0]=='-')?1:0;
        if(start==value.size()) { std::cout << "Error: " << field << " must be a whole number.\n"; return false; }
        for(size_t i=start;i<value.size();++i)
            if(!std::isdigit(static_cast<unsigned char>(value[i]))) {
                std::cout << "Error: " << field << " must be a whole number.\n"; return false;
            }
        try {
            size_t count=0;
            long long parsed=std::stoll(value,&count);
            if(count!=value.size()||parsed<INT_MIN||parsed>INT_MAX) {
                std::cout << "Error: " << field << " is outside the supported number range.\n";return false;
            }
            result=static_cast<int>(parsed);return true;
        } catch(...) { std::cout << "Error: " << field << " is outside the supported number range.\n";return false; }
    }
    bool required(const std::string& value,const std::string& field) {
        if(value.empty()||value.find_first_not_of(" \t")==std::string::npos) {
            std::cout << "Error: " << field << " is not entered.\n";return false;
        }
        if(value.size()>120||value.find('\n')!=std::string::npos) {
            std::cout << "Error: " << field << " must be 1-120 characters.\n";return false;
        }
        return true;
    }
    bool dateInput(const std::string& label,std::string& date) {
        date=ask(label);
        if(!required(date,label.substr(0,label.find(':'))))return false;
        if(!Library::validDate(date)) {
            std::cout << "Error: " << label.substr(0,label.find(':')) << " must be a valid YYYY-MM-DD date.\n";
            return false;
        }
        return true;
    }
    // Display the current book inventory.
    void books(const Library& db) {
        for(const auto& [id,b]:db.allBooks())std::cout << id<<" | "<<b.title<<" | "<<b.author<<" | "<<b.category<<" | "<<b.available<<"/"<<b.total<<"\n";
    }
    // Display account details without exposing password hashes.
    void users(const Library& db) {
        for(const auto& [id,u]:db.allUsers())std::cout << id<<" | "<<u.username<<" | "<<u.role<<" | "<<(u.active?"ACTIVE":"DISABLED")<<"\n";
    }
    // Display borrowing requests for staff review.
    void requests(const Library& db) {
        for(const auto& [id,r]:db.allRequests())std::cout << id<<" | book "<<r.bookId<<" | member "<<r.memberId<<" | "<<r.status<<"\n";
    }
    // Display loan history, optionally filtered to one member.
    void loans(const Library& db,int member=0) {
        for(const auto& [id,l]:db.allLoans())if(member==0||l.memberId==member)std::cout << id<<" | book "<<l.bookId<<" | member "<<l.memberId<<" | "<<l.issued<<" -> "<<l.due<<" | returned "<<(l.returned.empty()?"NO":l.returned)<<"\n";
    }
    // Display only the operations available to the signed-in role.
    void menu(const User& user) {
        std::cout << "\nLogged in: "<<user.username<<" ["<<user.role<<"]\n1 List books\n2 Search book\n";
        if(user.role=="ADMIN"||user.role=="STAFF")std::cout << "3 Add book\n4 Update book\n5 Delete book\n6 View requests\n7 Approve request\n8 Reject request\n9 Return book\n10 View loans\n";
        if(user.role=="ADMIN")std::cout << "11 Add user\n12 View users\n13 Enable/disable user\n";
        if(user.role=="MEMBER")std::cout << "3 Request book\n4 My requests\n5 My loans\n";
        std::cout << "0 Logout\n";
    }
}
    // Initialize storage, authenticate users, and dispatch menu actions.
int main() {
    const std::string path="data/library.records";
    Library db;
    if(!db.load(path)) {
        std::cerr << "Cannot load records: invalid or corrupted data file.\n";
        return 1;
    }
    if(!db.initialized()) {
        std::cout << "FIRST RUN: Create administrator (password at least 10 characters).\n";
        std::string firstUser=ask("Admin username: ");
        std::string firstPassword=askPassword("Admin password: ");
        if(!required(firstUser,"Admin username")||!db.createAdmin(firstUser,firstPassword)) {
            std::cerr << "Error: " << db.error() << "\n";
            return 1;
        }
        if(!db.save(path)) {
            std::cerr << "Error: Failed to save administrator records.\n";
            return 1;
        }
    }
    while(true) {
        std::cout << "\nLIBRARY MANAGEMENT SYSTEM\n";
        std::string name=ask("Username (or EXIT): ");
        if(name=="EXIT")break;
        std::string pass=askPassword("Password: ");
        const User* logged=db.login(name,pass);
        if(!logged) {
            if(name.empty())std::cout << "Error: Username is not entered.\n";
            else if(pass.empty())std::cout << "Error: Password is not entered.\n";
            else {
                bool found=false, disabled=false;
                for(const auto& [id,u]:db.allUsers()) { (void)id; if(u.username==name) {found=true;disabled=!u.active;break;} }
                std::cout << (disabled?"Error: Account is disabled.":found?"Error: Incorrect password.":"Error: Username does not exist.") << "\n";
            }
            continue;
        }
        int userId=logged->id;
        while(true) {
            auto u=db.allUsers().find(userId);
            if(u==db.allUsers().end()||!u->second.active)break;
            const User& actor=u->second;
            menu(actor);
            int choice=0;
            if(!number("Choice: ",choice))continue;
            if(choice==0)break;
            // Each role has its own sequential menu; map member selections to internal actions.
            if(actor.role=="MEMBER" && choice>=3 && choice<=5)choice+=11;
            bool changed=false,success=false;
            if(choice==1)books(db);
            else if(choice==2) {
                int id=0;
                if(!number("Book ID: ",id))continue;
                if(id<=0){std::cout << "Error: Book ID must be greater than zero.\n";continue;}
                auto it=db.allBooks().find(id);
                if(it==db.allBooks().end())std::cout << "Error: Book ID does not exist.\n";
                else std::cout << it->second.title << " | available " << it->second.available << "\n";
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&(choice==3||choice==4)) {
                Book b;
                if(!number("Book ID: ",b.id))continue;
                if(b.id<=0){std::cout << "Error: Book ID must be greater than zero.\n";continue;}
                // Check existence early so the user gets a relevant error immediately.
                if(choice==3&&db.allBooks().count(b.id)){std::cout << "Error: Book ID already exists.\n";continue;}
                if(choice==4&&!db.allBooks().count(b.id)){std::cout << "Error: Book ID does not exist.\n";continue;}
                b.title=ask("Title: ");if(!required(b.title,"Book title"))continue;
                b.author=ask("Author: ");if(!required(b.author,"Author"))continue;
                b.category=ask("Category: ");if(!required(b.category,"Category"))continue;
                if(!number("Total copies: ",b.total))continue;
                changed=true;success=choice==3?db.addBook(actor,b):db.updateBook(actor,b);
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==5) {
                int id=0;if(!number("Book ID: ",id))continue;
                changed=true;success=db.deleteBook(actor,id);
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==6)requests(db);
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==7) {
                int id=0;if(!number("Request ID: ",id))continue;
                std::string issued,due;
                if(!dateInput("Issue date YYYY-MM-DD: ",issued))continue;
                if(!dateInput("Due date YYYY-MM-DD: ",due))continue;
                changed=true;success=db.approveRequest(actor,id,issued,due);
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==8) {
                int id=0;if(!number("Request ID: ",id))continue;
                changed=true;success=db.rejectRequest(actor,id);
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==9) {
                int id=0;if(!number("Loan ID: ",id))continue;
                std::string date;if(!dateInput("Return date YYYY-MM-DD: ",date))continue;
                int fine=0;changed=true;success=db.returnBook(actor,id,date,fine);
                if(success)std::cout << "Fine: " << fine << "\n";
            }
            else if((actor.role=="ADMIN"||actor.role=="STAFF")&&choice==10)loans(db);
            else if(actor.role=="ADMIN"&&choice==11) {
                std::string username=ask("New username: ");if(!required(username,"New username"))continue;
                std::string password=askPassword("Password (10+ characters): ");
                if(password.empty()){std::cout << "Error: Password is not entered.\n";continue;}
                std::string role=ask("Role ADMIN/STAFF/MEMBER: ");
                if(!required(role,"Role"))continue;
                changed=true;success=db.addUser(actor,username,password,role);
            }
            else if(actor.role=="ADMIN"&&choice==12)users(db);
            else if(actor.role=="ADMIN"&&choice==13) {
                int id=0,active=0;
                if(!number("User ID: ",id)||!number("Enable=1, disable=0: ",active))continue;
                if(active!=0&&active!=1){std::cout << "Error: Enter 1 to enable or 0 to disable.\n";continue;}
                changed=true;success=db.setUserActive(actor,id,active==1);
            }
            else if(actor.role=="MEMBER"&&choice==14) {
                int id=0;if(!number("Book ID: ",id))continue;
                changed=true;success=db.requestBook(actor,id);
            }
            else if(actor.role=="MEMBER"&&choice==15) {
                for(const auto& [id,r]:db.allRequests())if(r.memberId==actor.id)std::cout << id << " | book " << r.bookId << " | " << r.status << "\n";
            }
            else if(actor.role=="MEMBER"&&choice==16)loans(db,actor.id);
            else std::cout << "Error: Invalid menu option or access denied for your role.\n";
            if(changed) {
                std::cout << (success?"Success":"Error: ") << (success?"":db.error()) << "\n";
                if(success&&!db.save(path)) {
                    std::cerr << "Error: Unable to save records to " << path << ". Stop to avoid data loss.\n";
                    return 1;
                }
            }
        }
    }
    return 0;
}
