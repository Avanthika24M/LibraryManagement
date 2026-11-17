#include "library.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <string>
using namespace lms;

class LibraryTest : public ::testing::Test {
protected:
    Library db;
    User admin, staff, member;
    void SetUp() override {
        ASSERT_TRUE(db.createAdmin("admin", "adminpassword123"));
        admin = *db.login("admin", "adminpassword123");
        ASSERT_TRUE(db.addUser(admin,"staff","staffpassword123","STAFF"));
        ASSERT_TRUE(db.addUser(admin,"member","memberpassword123","MEMBER"));
        staff = *db.login("staff","staffpassword123");
        member = *db.login("member","memberpassword123");
    }
    Book book(int id=101,int total=3) { return {id,"C++ Book","Author","Programming",total,0}; }
    int requestId() const { return db.allRequests().rbegin()->first; }
    int loanId() const { return db.allLoans().rbegin()->first; }
};

TEST_F(LibraryTest, LoginCorrectIncorrectAndDisabled) {
    EXPECT_NE(db.login("admin","adminpassword123"),nullptr);
    EXPECT_EQ(db.login("admin","wrong"),nullptr);
    EXPECT_EQ(db.login("unknown","anything"),nullptr);
    ASSERT_TRUE(db.setUserActive(admin,member.id,false));
    EXPECT_EQ(db.login("member","memberpassword123"),nullptr);
    EXPECT_TRUE(db.setUserActive(admin,member.id,true));
}
TEST_F(LibraryTest, InitialAdminAndUserValidation) {
    EXPECT_FALSE(db.createAdmin("another","password123"));
    EXPECT_EQ(db.error(),"Administrator already exists.");
    EXPECT_FALSE(db.addUser(member,"x","password123","MEMBER"));
    EXPECT_FALSE(db.addUser(admin,"","password123","MEMBER"));
    EXPECT_FALSE(db.addUser(admin,"other","short","MEMBER"));
    EXPECT_FALSE(db.addUser(admin,"other","password123","INVALID"));
    EXPECT_FALSE(db.addUser(admin,"member","password123","MEMBER"));
    EXPECT_EQ(db.error(),"Username already exists.");
    EXPECT_FALSE(db.setUserActive(admin,admin.id,false));
    EXPECT_FALSE(db.setUserActive(admin,999,false));
}
TEST_F(LibraryTest, AddBookValidationAndPermissions) {
    EXPECT_FALSE(db.addBook(member,book()));
    EXPECT_EQ(db.error(),"Only Admin or Staff can add books.");
    EXPECT_FALSE(db.addBook(admin,book(0)));
    EXPECT_EQ(db.error(),"Book ID must be greater than zero.");
    Book b=book(); b.title="";
    EXPECT_FALSE(db.addBook(admin,b));
    b=book();b.author="";EXPECT_FALSE(db.addBook(admin,b));
    b=book();b.category="";EXPECT_FALSE(db.addBook(admin,b));
    EXPECT_FALSE(db.addBook(admin,book(1,-1)));
    EXPECT_FALSE(db.addBook(admin,book(1,100001)));
    EXPECT_TRUE(db.addBook(staff,book()));
    EXPECT_FALSE(db.addBook(admin,book()));
    EXPECT_EQ(db.error(),"Book ID already exists.");
    EXPECT_EQ(db.allBooks().at(101).available,3);
}
TEST_F(LibraryTest, UpdateAndDeleteValidation) {
    EXPECT_FALSE(db.updateBook(admin,book()));
    EXPECT_FALSE(db.deleteBook(admin,101));
    ASSERT_TRUE(db.addBook(admin,book()));
    EXPECT_FALSE(db.updateBook(member,book()));
    EXPECT_FALSE(db.deleteBook(member,101));
    Book b=book();b.total=5;
    EXPECT_TRUE(db.updateBook(staff,b));
    EXPECT_EQ(db.allBooks().at(101).available,5);
    EXPECT_TRUE(db.deleteBook(staff,101));
    EXPECT_FALSE(db.deleteBook(admin,101));
}
TEST_F(LibraryTest, RequestApprovalAndReturn) {
    ASSERT_TRUE(db.addBook(admin,book(101,1)));
    EXPECT_FALSE(db.requestBook(admin,101));
    EXPECT_FALSE(db.requestBook(member,999));
    ASSERT_TRUE(db.requestBook(member,101));
    EXPECT_FALSE(db.requestBook(member,101));
    EXPECT_EQ(db.error(),"You already have a pending request for this book.");
    int id=requestId();
    EXPECT_FALSE(db.approveRequest(member,id,"2026-11-14","2026-11-28"));
    EXPECT_FALSE(db.approveRequest(admin,id,"bad","2026-11-28"));
    EXPECT_FALSE(db.approveRequest(admin,id,"2026-11-14","bad"));
    EXPECT_FALSE(db.approveRequest(admin,id,"2026-11-28","2026-11-14"));
    ASSERT_TRUE(db.approveRequest(admin,id,"2026-11-14","2026-11-28"));
    EXPECT_EQ(db.allBooks().at(101).available,0);
    EXPECT_FALSE(db.approveRequest(admin,id,"2026-11-14","2026-11-28"));
    EXPECT_FALSE(db.rejectRequest(admin,id));
    EXPECT_FALSE(db.requestBook(member,101));
    EXPECT_FALSE(db.deleteBook(admin,101));
    Book b=book(101,0);EXPECT_FALSE(db.updateBook(admin,b));
    int fine=-1;
    EXPECT_FALSE(db.returnBook(member,loanId(),"2026-11-29",fine));
    EXPECT_FALSE(db.returnBook(admin,loanId(),"invalid",fine));
    EXPECT_FALSE(db.returnBook(admin,loanId(),"2026-11-13",fine));
    ASSERT_TRUE(db.returnBook(staff,loanId(),"2026-11-30",fine));
    EXPECT_EQ(fine,10);
    EXPECT_EQ(db.allBooks().at(101).available,1);
    EXPECT_FALSE(db.returnBook(admin,loanId(),"2026-11-30",fine));
    EXPECT_FALSE(db.deleteBook(admin,101)); // preserve history
}
TEST_F(LibraryTest, RejectionAndInactiveMember) {
    ASSERT_TRUE(db.addBook(admin,book()));
    ASSERT_TRUE(db.requestBook(member,101));
    int id=requestId();
    EXPECT_FALSE(db.rejectRequest(member,id));
    EXPECT_FALSE(db.rejectRequest(admin,999));
    ASSERT_TRUE(db.rejectRequest(staff,id));
    EXPECT_FALSE(db.rejectRequest(staff,id));
    ASSERT_TRUE(db.requestBook(member,101));
    ASSERT_TRUE(db.setUserActive(admin,member.id,false));
    EXPECT_FALSE(db.approveRequest(admin,requestId(),"2026-11-14","2026-11-28"));
}
TEST(LibraryDateTest, CalendarAndFineBoundaries) {
    EXPECT_TRUE(Library::validDate("2024-02-29"));
    EXPECT_FALSE(Library::validDate("2025-02-29"));
    EXPECT_FALSE(Library::validDate("2026-13-01"));
    EXPECT_FALSE(Library::validDate("2026-00-01"));
    EXPECT_FALSE(Library::validDate("2026-11-31"));
    EXPECT_FALSE(Library::validDate("2026/11/17"));
    EXPECT_FALSE(Library::validDate(""));
    EXPECT_FALSE(Library::validDate("1999-12-31"));
    EXPECT_FALSE(Library::validDate("2101-01-01"));
    EXPECT_EQ(Library::overdueFine("2026-11-15","2026-11-15"),0);
    EXPECT_EQ(Library::overdueFine("2026-11-15","2026-11-14"),0);
    EXPECT_EQ(Library::overdueFine("2026-11-15","2026-11-18"),15);
    EXPECT_EQ(Library::overdueFine("bad","2026-11-18"),-1);
}
TEST_F(LibraryTest, PersistenceAndNextIdentifiers) {
    ASSERT_TRUE(db.addBook(admin,book()));
    ASSERT_TRUE(db.requestBook(member,101));
    const auto path=(std::filesystem::temp_directory_path()/"lms_gtest_records.txt").string();
    ASSERT_TRUE(db.save(path));
    Library restored;
    ASSERT_TRUE(restored.load(path));
    EXPECT_EQ(restored.allBooks().size(),1U);
    EXPECT_EQ(restored.allRequests().size(),1U);
    EXPECT_NE(restored.login("member","memberpassword123"),nullptr);
    const User newMember=*restored.login("member","memberpassword123");
    ASSERT_TRUE(restored.requestBook(newMember,101)==false); // duplicate pending
    ASSERT_TRUE(restored.addBook(*restored.login("admin","adminpassword123"),book(102)));
    ASSERT_TRUE(restored.requestBook(newMember,102));
    EXPECT_EQ(restored.allRequests().rbegin()->first,2);
    std::filesystem::remove(path);
}
TEST(LibraryStorageTest, MissingAndMalformedFiles) {
    auto path=(std::filesystem::temp_directory_path()/"lms_gtest_invalid_records.txt").string();
    std::filesystem::remove(path);
    Library db;
    EXPECT_TRUE(db.load(path));
    {std::ofstream out(path);out<<"INVALID\n";}
    EXPECT_FALSE(db.load(path));
    {std::ofstream out(path);out<<"LMS2\nB 1 \"Book\" \"Author\" \"Category\" 1 2\n";}
    EXPECT_FALSE(db.load(path));
    {std::ofstream out(path);out<<"LMS2\nX 1\n";}
    EXPECT_FALSE(db.load(path));
    {std::ofstream out(path);out<<"LMS2\nR 1 999 999 \"PENDING\"\n";}
    EXPECT_FALSE(db.load(path));
    std::filesystem::remove(path);
}
TEST(LibrarySecurityTest, HashAndSalt) {
    auto a=newSalt(),b=newSalt();
    EXPECT_FALSE(a.empty());
    EXPECT_FALSE(b.empty());
    EXPECT_EQ(hashPassword("password123",a),hashPassword("password123",a));
    EXPECT_NE(hashPassword("password123",a),hashPassword("different",a));
    EXPECT_NE(hashPassword("password123",a),hashPassword("password123",b));
}
