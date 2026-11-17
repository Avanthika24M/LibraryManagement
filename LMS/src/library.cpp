// Library Management System - library.cpp
// C++20 implementation; no external database or testing dependencies.

#include "library.hpp"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
namespace lms {
    namespace {
    // Check whether an active account has staff-level permissions.
        bool privileged(const User& u) {
            return u.active&&(u.role=="ADMIN"||u.role=="STAFF");
        }
    // Check whether an active account has administrator permissions.
        bool admin(const User& u) {
            return u.active&&u.role=="ADMIN";
        }
    // Reject empty, oversized, or multiline record fields.
        bool validText(const std::string& s) {
            return !s.empty()&&s.size()<=120&&s.find('\n')==std::string::npos;
        }
    // Convert a validated date into a day count.
        int days(const std::string& date) {
            int y=0,m=0,d=0;
            char x=0,z=0;
            std::istringstream in(date);
            in>>y>>x>>m>>z>>d;
            std::tm tm {
            };
            tm.tm_year=y-1900;
            tm.tm_mon=m-1;
            tm.tm_mday=d;
            tm.tm_hour=12;
            auto t=std::mktime(&tm);
            return static_cast<int>(t/86400);
        }
    }
    // Validate a date in YYYY-MM-DD format.
    bool Library::validDate(const std::string& date) {
        if(date.size()!=10||date[4]!='-'||date[7]!='-')return false;
        for(size_t i=0;i<date.size();++i)if(i!=4&&i!=7&&(date[i]<'0'||date[i]>'9'))return false;
        int y=std::stoi(date.substr(0,4)),m=std::stoi(date.substr(5,2)),d=std::stoi(date.substr(8,2));
        if(y<2000||y>2100||m<1||m>12||d<1||d>31)return false;
        std::tm tm {
        };
        tm.tm_year=y-1900;
        tm.tm_mon=m-1;
        tm.tm_mday=d;
        tm.tm_hour=12;
        if(std::mktime(&tm)==-1)return false;
        return tm.tm_year==y-1900&&tm.tm_mon==m-1&&tm.tm_mday==d;
    }
    // Calculate the late fee at five units per overdue day.
    int Library::overdueFine(const std::string& due,const std::string& returned) {
        if(!validDate(due)||!validDate(returned))return -1;
        return std::max(0,days(returned)-days(due))*5;
    }
    // Create the initial administrator account on first launch.
    bool Library::createAdmin(const std::string& username,const std::string& password) {
        clearError();
        if(!users.empty()) return fail("Administrator already exists.");
        if(!validText(username)) return fail("Admin username is required (1-120 characters, no newline).");
        if(password.size()<10) return fail("Admin password must contain at least 10 characters.");
        if(password.size()>128) return fail("Admin password must not exceed 128 characters.");
        User u;u.id=nextUser++;u.username=username;u.salt=newSalt();u.passwordHash=hashPassword(password,u.salt);u.role="ADMIN";users[u.id]=u;return true;
    }
    const User* Library::login(const std::string& username,const std::string& password)const {
        for(const auto& [id,u]:users) {
            (void)id;
            if(u.username==username&&u.active) {
                auto candidate=hashPassword(password,u.salt);
                unsigned diff=0;
                for(size_t i=0;i<candidate.size()&&i<u.passwordHash.size();++i)diff|=static_cast<unsigned>(candidate[i]^u.passwordHash[i]);
                if(diff==0&&candidate.size()==u.passwordHash.size())return &u;
            }
        }
        return nullptr;
    }
    bool Library::addUser(const User& actor,const std::string& username,const std::string& password,const std::string& role) {
        clearError();
        if(!admin(actor))return fail("Only an active Admin can create users.");
        if(!validText(username))return fail("Username is required (1-120 characters, no newline).");
        if(password.size()<10)return fail("Password must contain at least 10 characters.");
        if(password.size()>128)return fail("Password must not exceed 128 characters.");
        if(role!="ADMIN"&&role!="STAFF"&&role!="MEMBER")return fail("Role must be ADMIN, STAFF, or MEMBER.");
        for(const auto& [id,u]:users){(void)id;if(u.username==username)return fail("Username already exists.");}
        User u;u.id=nextUser++;u.username=username;u.salt=newSalt();u.passwordHash=hashPassword(password,u.salt);u.role=role;users[u.id]=u;return true;
    }
    bool Library::setUserActive(const User& actor,int userId,bool active) {
        clearError();
        if(!admin(actor))return fail("Only an active Admin can change user status.");
        if(actor.id==userId)return fail("You cannot change your own account status.");
        if(!users.count(userId))return fail("User ID does not exist.");
        users[userId].active=active;return true;
    }
    bool Library::addBook(const User& actor,const Book& b) {
        clearError();
        if(!privileged(actor))return fail("Only Admin or Staff can add books.");
        if(b.id<=0)return fail("Book ID must be greater than zero.");
        if(books.count(b.id))return fail("Book ID already exists.");
        if(!validText(b.title))return fail("Book title is required (1-120 characters, no newline).");
        if(!validText(b.author))return fail("Author is required (1-120 characters, no newline).");
        if(!validText(b.category))return fail("Category is required (1-120 characters, no newline).");
        if(b.total<0)return fail("Total copies cannot be negative.");
        if(b.total>100000)return fail("Total copies cannot exceed 100000.");
        Book copy=b;copy.available=b.total;books[b.id]=copy;return true;
    }
    bool Library::updateBook(const User& actor,const Book& b) {
        clearError();
        if(!privileged(actor))return fail("Only Admin or Staff can update books.");
        if(b.id<=0)return fail("Book ID must be greater than zero.");
        auto it=books.find(b.id);
        if(it==books.end())return fail("Book ID does not exist.");
        if(!validText(b.title))return fail("Book title is required (1-120 characters, no newline).");
        if(!validText(b.author))return fail("Author is required (1-120 characters, no newline).");
        if(!validText(b.category))return fail("Category is required (1-120 characters, no newline).");
        if(b.total<0)return fail("Total copies cannot be negative.");
        if(b.total>100000)return fail("Total copies cannot exceed 100000.");
        int borrowed=it->second.total-it->second.available;
        if(b.total<borrowed)return fail("Total copies cannot be less than currently borrowed copies.");
        Book copy=b;copy.available=b.total-borrowed;it->second=copy;return true;
    }
    bool Library::deleteBook(const User& actor,int bookId) {
        clearError();
        if(!privileged(actor))return fail("Only Admin or Staff can delete books.");
        if(bookId<=0)return fail("Book ID must be greater than zero.");
        auto it=books.find(bookId);
        if(it==books.end())return fail("Book ID does not exist.");
        if(it->second.total!=it->second.available)return fail("Book has borrowed copies and cannot be deleted.");
        for(const auto& [id,r]:requests){(void)id;if(r.bookId==bookId)return fail("Book has request history and cannot be deleted.");}
        for(const auto& [id,l]:loans){(void)id;if(l.bookId==bookId)return fail("Book has loan history and cannot be deleted.");}
        books.erase(it);return true;
    }
    bool Library::requestBook(const User& actor,int bookId) {
        clearError();
        if(!actor.active||actor.role!="MEMBER")return fail("Only an active Member can request books.");
        if(bookId<=0)return fail("Book ID must be greater than zero.");
        auto b=books.find(bookId);
        if(b==books.end())return fail("Book ID does not exist.");
        if(b->second.available<=0)return fail("No available copies of this book.");
        for(const auto& [id,r]:requests){(void)id;if(r.bookId==bookId&&r.memberId==actor.id&&r.status=="PENDING")return fail("You already have a pending request for this book.");}
        Request r;r.id=nextRequest++;r.bookId=bookId;r.memberId=actor.id;requests[r.id]=r;return true;
    }
    bool Library::approveRequest(const User& actor,int requestId,const std::string& issued,const std::string& due) {
        clearError();
        if(!privileged(actor))return fail("Only Admin or Staff can approve requests.");
        if(requestId<=0)return fail("Request ID must be greater than zero.");
        auto it=requests.find(requestId);
        if(it==requests.end())return fail("Request ID does not exist.");
        if(it->second.status!="PENDING")return fail("Request is not pending (already approved or rejected).");
        if(!validDate(issued))return fail("Issue date must be a valid date in YYYY-MM-DD format.");
        if(!validDate(due))return fail("Due date must be a valid date in YYYY-MM-DD format.");
        if(due<issued)return fail("Due date cannot be earlier than issue date.");
        auto b=books.find(it->second.bookId);
        auto m=users.find(it->second.memberId);
        if(b==books.end())return fail("Requested book no longer exists.");
        if(b->second.available<=0)return fail("No copies available for approval.");
        if(m==users.end())return fail("Request member no longer exists.");
        if(!m->second.active)return fail("Member account is disabled.");
        Loan loan;loan.id=nextLoan++;loan.bookId=b->first;loan.memberId=m->first;loan.issued=issued;loan.due=due;loans[loan.id]=loan;--b->second.available;it->second.status="APPROVED";return true;
    }
    bool Library::rejectRequest(const User& actor,int requestId) {
        clearError();
        if(!privileged(actor))return fail("Only Admin or Staff can reject requests.");
        if(requestId<=0)return fail("Request ID must be greater than zero.");
        auto it=requests.find(requestId);
        if(it==requests.end())return fail("Request ID does not exist.");
        if(it->second.status!="PENDING")return fail("Request is not pending (already approved or rejected).");
        it->second.status="REJECTED";return true;
    }
    bool Library::returnBook(const User& actor,int loanId,const std::string& returned,int& fine) {
        clearError();
        if(!privileged(actor))return fail("Only Admin or Staff can record returns.");
        if(loanId<=0)return fail("Loan ID must be greater than zero.");
        auto it=loans.find(loanId);
        if(it==loans.end())return fail("Loan ID does not exist.");
        if(!it->second.returned.empty())return fail("Book has already been returned.");
        if(!validDate(returned))return fail("Return date must be a valid date in YYYY-MM-DD format.");
        if(returned<it->second.issued)return fail("Return date cannot be earlier than issue date.");
        auto b=books.find(it->second.bookId);
        if(b==books.end())return fail("Loan book no longer exists.");
        fine=overdueFine(it->second.due,returned);it->second.returned=returned;++b->second.available;return true;
    }
}
