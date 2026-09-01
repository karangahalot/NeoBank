// ============================================================
//  BANK MANAGEMENT SYSTEM  (with Transaction History)
//  Language : C++
//  Storage  : Binary flat files
//    accounts.dat       – account records
//    personalinfo.dat   – password records
//    transactions.dat   – transaction log records
// ============================================================

#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>       // for tolower() used in case-insensitive search
#include <limits>
#include <ctime>        // for time(), localtime(), strftime()
using namespace std;

// ─────────────────────────────────────────────────────────────
//  STRUCT: account
//  Stores all information about one bank account.
//  Fields:
//    no      – unique account number (starts at 10001)
//    active  – 1 = open, 0 = closed
//    name    – account holder's full name
//    phone   – 10-digit mobile number (11 bytes: digits + '\0')
//    email   – email address
//    aadhar  – Aadhaar ID number
//    balance – current balance in rupees
// ─────────────────────────────────────────────────────────────
struct account {
    int    no;
    int    active;
    char   name[50];
    char   phone[11];
    char   email[50];
    char   aadhar[20];
    double balance;
};

// ─────────────────────────────────────────────────────────────
//  STRUCT: pinfo  (password info)
//  Kept in a separate file so account data doesn't expose passwords.
//  Fields:
//    no       – links to account.no
//    password – login password (up to 49 chars)
// ─────────────────────────────────────────────────────────────
struct pinfo {
    int  no;
    char password[50];
};

// ─────────────────────────────────────────────────────────────
//  STRUCT: txn  (transaction record)
//  Every deposit, withdrawal, and account-open event is logged
//  as one txn record appended to transactions.dat.
//  Fields:
//    accno   – which account this transaction belongs to
//    type    – "DEPOSIT", "WITHDRAW", or "ACCOUNT OPENED"
//    amount  – rupee amount involved
//    balance – balance AFTER this transaction
//    when    – human-readable timestamp ("DD/MM/YYYY HH:MM:SS")
// ─────────────────────────────────────────────────────────────
struct txn {
    int    accno;
    char   type[20];    // "DEPOSIT" / "WITHDRAW" / "ACCOUNT OPENED"
    double amount;
    double balance;
    char   when[25];    // "DD/MM/YYYY HH:MM:SS"
};

// ─────────────────────────────────────────────────────────────
//  CLASS: bank
//  All banking operations are member functions.
//  Private section: low-level file helpers.
//  Public  section: menu-callable operations.
// ─────────────────────────────────────────────────────────────
class bank {

    // Persistent storage file names
    const char* acf  = "accounts.dat";
    const char* pass = "personalinfo.dat";
    const char* txf  = "transactions.dat";  // NEW: transaction log

    // ═══════════════════════════════════════
    //  PRIVATE HELPERS
    // ═══════════════════════════════════════

    // ─────────────────────────────────────────────────────────
    //  nowtimestamp(buf, size)
    //  PURPOSE : Fill buf with the current date and time as a string.
    //  HOW     : Uses C standard library time() + localtime() +
    //            strftime() to format the current system clock.
    //  RESULT  : buf contains e.g. "21/05/2025 14:30:05"
    // ─────────────────────────────────────────────────────────
    void nowtimestamp(char* buf, int size) {
        time_t now = time(nullptr);             // get current epoch seconds
        struct tm* lt = localtime(&now);        // convert to local date/time
        strftime(buf, size, "%d/%m/%Y %H:%M:%S", lt);  // format as string
    }

    // ─────────────────────────────────────────────────────────
    //  logtxn(accno, type, amount, balanceAfter)
    //  PURPOSE : Append a new transaction record to transactions.dat.
    //  HOW     : Builds a txn struct, stamps the current time,
    //            and appends it in binary mode. Called after every
    //            deposit, withdrawal, or account creation.
    //  PARAMS  : accno        – account number
    //            type         – "DEPOSIT" / "WITHDRAW" / "ACCOUNT OPENED"
    //            amount       – the transaction amount
    //            balanceAfter – balance after the transaction
    // ─────────────────────────────────────────────────────────
    void logtxn(int accno, const char* type, double amount, double balanceAfter) {
        txn t;
        memset(&t, 0, sizeof(t));
        t.accno   = accno;
        t.amount  = amount;
        t.balance = balanceAfter;
        strncpy(t.type, type, sizeof(t.type) - 1);  // copy type safely
        nowtimestamp(t.when, sizeof(t.when));         // stamp current time
        ofstream f(txf, ios::app | ios::binary);      // open for append
        f.write((char*)&t, sizeof(t));                // write the record
    }

    // ─────────────────────────────────────────────────────────
    //  nextno()
    //  PURPOSE : Generate the next unique account number.
    //  HOW     : Reads every record in accounts.dat and tracks
    //            the highest account number seen, returns that + 1.
    //            Returns 10001 if the file doesn't exist yet.
    // ─────────────────────────────────────────────────────────
    int nextno() {
        ifstream f(acf, ios::binary);
        if (!f) return 10001;
        int last = 10000;
        account a;
        while (f.read((char*)&a, sizeof(a)))
            if (a.no > last) last = a.no;
        return last + 1;
    }

    // ─────────────────────────────────────────────────────────
    //  getaccount(no, a)
    //  PURPOSE : Find and load an account record by account number.
    //  HOW     : Reads accounts.dat sequentially until it finds
    //            a record where a.no == no; copies it into 'a'.
    //  RETURNS : true if found, false otherwise
    // ─────────────────────────────────────────────────────────
    bool getaccount(int no, account& a) {
        ifstream f(acf, ios::binary);
        if (!f) return false;
        while (f.read((char*)&a, sizeof(a)))
            if (a.no == no) return true;
        return false;
    }

    // ─────────────────────────────────────────────────────────
    //  saveaccount(a)
    //  PURPOSE : Overwrite an existing account record in-place.
    //  HOW     : Opens file for read+write, seeks to the position
    //            of the matching record, and overwrites it.
    //            Other records in the file are untouched.
    // ─────────────────────────────────────────────────────────
    void saveaccount(account& a) {
        fstream f(acf, ios::in | ios::out | ios::binary);
        if (!f) return;
        account t;
        while (f.read((char*)&t, sizeof(t)))
            if (t.no == a.no) {
                f.seekp(-(long)sizeof(t), ios::cur); // step back one record
                f.write((char*)&a, sizeof(a));        // overwrite
                return;
            }
    }

    // ─────────────────────────────────────────────────────────
    //  getpinfo(no, c) / savepinfo(c)
    //  Same find-and-overwrite pattern as getaccount/saveaccount
    //  but operates on the personalinfo.dat file.
    // ─────────────────────────────────────────────────────────
    bool getpinfo(int no, pinfo& c) {
        ifstream f(pass, ios::binary);
        if (!f) return false;
        while (f.read((char*)&c, sizeof(c)))
            if (c.no == no) return true;
        return false;
    }

    void savepinfo(pinfo& c) {
        fstream f(pass, ios::in | ios::out | ios::binary);
        if (!f) return;
        pinfo t;
        while (f.read((char*)&t, sizeof(t)))
            if (t.no == c.no) {
                f.seekp(-(long)sizeof(t), ios::cur);
                f.write((char*)&c, sizeof(c));
                return;
            }
    }

    // ─────────────────────────────────────────────────────────
    //  validphone(ph)
    //  PURPOSE : Ensure a phone number is exactly 10 digits.
    //  RETURNS : true if valid, false otherwise
    // ─────────────────────────────────────────────────────────
    bool validphone(const char* ph) {
        if (strlen(ph) != 10) return false;
        for (int i = 0; i < 10; i++)
            if (ph[i] < '0' || ph[i] > '9') return false;
        return true;
    }

    // ─────────────────────────────────────────────────────────
    //  clearinput()
    //  PURPOSE : Clear cin error flags and discard the current line.
    //  WHY     : After "cin >> x", a '\n' remains in the buffer.
    //            Without this, the next cin.getline() reads an empty
    //            string. Also recovers from non-numeric input errors.
    // ─────────────────────────────────────────────────────────
    void clearinput() {
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }

    // ─────────────────────────────────────────────────────────
    //  login(no, a)
    //  PURPOSE : Authenticate a user before any sensitive operation.
    //  HOW     :
    //    1. Read account number and password from the user.
    //    2. Look up the pinfo record → check password matches.
    //    3. Look up the account record → check it exists and is active.
    //  RETURNS : true only when all three checks pass.
    // ─────────────────────────────────────────────────────────
    bool login(int& no, account& a) {
        cout << "\n Enter account number : ";
        cin  >> no;
        char p[50];
        cout << " Enter password       : ";
        cin  >> p;

        pinfo c;
        if (!getpinfo(no, c)) {
            cout << "\n Account not found. Please check the account number.\n";
            return false;
        }
        if (strcmp(c.password, p) != 0) {
            cout << "\n Incorrect password. Please try again.\n";
            return false;
        }
        if (!getaccount(no, a) || !a.active) {
            cout << "\n This account is closed or does not exist.\n";
            return false;
        }
        return true;
    }

    // ─────────────────────────────────────────────────────────
    //  showaccount(a)
    //  PURPOSE : Print all fields of an account in a neat block.
    // ─────────────────────────────────────────────────────────
    void showaccount(const account& a) {
        cout << " ----------------------------------------\n";
        cout << " Account number : " << a.no         << "\n";
        cout << " Name           : " << a.name       << "\n";
        cout << " Phone          : " << a.phone      << "\n";
        cout << " Email          : " << a.email      << "\n";
        cout << " Aadhaar        : " << a.aadhar     << "\n";
        cout << " Balance        : Rs." << a.balance << "\n";
        cout << " ----------------------------------------\n";
    }


public:
    // ═══════════════════════════════════════
    //  PUBLIC MENU FUNCTIONS
    // ═══════════════════════════════════════

    // ─────────────────────────────────────────────────────────
    //  createaccount()
    //  PURPOSE : Open a new bank account.
    //  HOW     :
    //    1. Collect name, phone (validated), email, Aadhaar.
    //    2. Ask for opening deposit (minimum Rs.1000).
    //    3. Ask for password (confirmed twice).
    //    4. Assign account number via nextno().
    //    5. Write account struct to accounts.dat.
    //    6. Write pinfo struct to personalinfo.dat.
    //    7. Log an "ACCOUNT OPENED" transaction.
    // ─────────────────────────────────────────────────────────
    void createaccount() {
        cout << "\n === CREATE A NEW BANK ACCOUNT ===\n";

        account a;
        pinfo   c;
        memset(&a, 0, sizeof(a));
        memset(&c, 0, sizeof(c));

        clearinput();   // flush '\n' left by menu's "cin >> ch"

        cout << " Full name       : ";
        cin.getline(a.name, 50);

        do {
            cout << " Phone number    : ";
            cin >> a.phone;
            if (!validphone(a.phone))
                cout << " Invalid phone number. Must be exactly 10 digits.\n";
        } while (!validphone(a.phone));
        clearinput();

        cout << " Email address   : ";
        cin.getline(a.email, 50);

        cout << " Aadhaar number  : ";
        cin.getline(a.aadhar, 20);

        cout << "\n Minimum deposit required: Rs.1000\n";
        cout << " Amount to deposit : Rs.";
        while (!(cin >> a.balance)) {
            cout << " Invalid amount. Enter a number: Rs.";
            clearinput();
        }
        if (a.balance < 1000) {
            cout << " Deposited less than Rs.1000. Account not created.\n";
            return;
        }

        char p1[50], p2[50];
        cout << "\n Set a password    : ";
        cin >> p1;
        cout << " Confirm password  : ";
        cin >> p2;
        if (strcmp(p1, p2) != 0) {
            cout << "\n Passwords do not match. Account not created.\n";
            return;
        }

        a.no     = nextno();
        a.active = 1;
        c.no     = a.no;
        strcpy(c.password, p1);

        { ofstream fa(acf,  ios::app | ios::binary); fa.write((char*)&a, sizeof(a)); }
        { ofstream fp(pass, ios::app | ios::binary); fp.write((char*)&c, sizeof(c)); }

        // Log the opening deposit as the first transaction
        logtxn(a.no, "ACCOUNT OPENED", a.balance, a.balance);

        cout << "\n Account created successfully!\n";
        cout << " Your account number is : " << a.no << "\n";
    }

    // ─────────────────────────────────────────────────────────
    //  deposit()
    //  PURPOSE : Credit money into the account.
    //  HOW     :
    //    1. Authenticate via login().
    //    2. Read deposit amount (must be positive).
    //    3. Add to balance and save.
    //    4. Log a "DEPOSIT" transaction.
    // ─────────────────────────────────────────────────────────
    void deposit() {
        cout << "\n === DEPOSIT MONEY ===\n";
        int no; account a;
        if (!login(no, a)) return;

        cout << "\n Current balance : Rs." << a.balance << "\n";
        cout << " Amount to deposit : Rs.";
        double amt;
        while (!(cin >> amt) || amt <= 0) {
            cout << " Please enter a valid positive amount: Rs.";
            clearinput();
        }

        a.balance += amt;
        saveaccount(a);
        logtxn(a.no, "DEPOSIT", amt, a.balance);   // record the transaction

        cout << "\n Rs." << amt << " deposited successfully!\n";
        cout << " New balance : Rs." << a.balance << "\n";
    }

    // ─────────────────────────────────────────────────────────
    //  withdraw()
    //  PURPOSE : Debit money from the account.
    //  HOW     :
    //    1. Authenticate via login().
    //    2. Read withdrawal amount (must be positive).
    //    3. Enforce minimum balance rule (Rs.1000 must remain).
    //    4. Subtract and save.
    //    5. Log a "WITHDRAW" transaction.
    // ─────────────────────────────────────────────────────────
    void withdraw() {
        cout << "\n === WITHDRAW MONEY ===\n";
        int no; account a;
        if (!login(no, a)) return;

        cout << "\n Current balance : Rs." << a.balance << "\n";
        cout << " Amount to withdraw : Rs.";
        double amt;
        while (!(cin >> amt) || amt <= 0) {
            cout << " Please enter a valid positive amount: Rs.";
            clearinput();
        }

        if ((a.balance - amt) < 1000) {
            cout << "\n Cannot withdraw. Minimum balance of Rs.1000 must remain.\n";
            cout << " Maximum you can withdraw : Rs." << (a.balance - 1000) << "\n";
            return;
        }

        a.balance -= amt;
        saveaccount(a);
        logtxn(a.no, "WITHDRAW", amt, a.balance);  // record the transaction

        cout << "\n Rs." << amt << " withdrawn successfully!\n";
        cout << " New balance : Rs." << a.balance << "\n";
    }

    // ─────────────────────────────────────────────────────────
    //  checkbalance()
    //  PURPOSE : Display full account details. Read-only.
    //  HOW     : Authenticates, then calls showaccount().
    // ─────────────────────────────────────────────────────────
    void checkbalance() {
        cout << "\n === ACCOUNT DETAILS ===\n";
        int no; account a;
        if (!login(no, a)) return;
        cout << "\n Your details:\n";
        showaccount(a);
    }

    // ─────────────────────────────────────────────────────────
    //  history()
    //  PURPOSE : Show transaction history for an account.
    //  HOW     :
    //    1. Authenticate via login().
    //    2. Read transactions.dat from the beginning.
    //    3. Print every txn whose accno matches the logged-in account.
    //    4. Counts and shows total transactions found.
    //    If no transactions exist yet, says so.
    // ─────────────────────────────────────────────────────────
    void history() {
        cout << "\n === TRANSACTION HISTORY ===\n";
        int no; account a;
        if (!login(no, a)) return;

        ifstream f(txf, ios::binary);
        if (!f) {
            cout << "\n No transaction history found.\n";
            return;
        }

        cout << "\n Account: " << a.no << "  |  Holder: " << a.name << "\n";
        cout << " ================================================================\n";
        cout << " No. | Date & Time          | Type           | Amount    | Balance\n";
        cout << " ================================================================\n";

        txn t;
        int count = 0;
        // Read every record and print those belonging to this account
        while (f.read((char*)&t, sizeof(t))) {
            if (t.accno == no) {
                count++;
                // Print a formatted row for each transaction
                cout << " " << count
                     << "   | " << t.when
                     << " | ";

                // Pad the type field to keep columns aligned
                cout << t.type;
                int pad = 15 - (int)strlen(t.type);
                for (int i = 0; i < pad; i++) cout << ' ';

                cout << "| Rs." << t.amount
                     << "    | Rs." << t.balance << "\n";
            }
        }

        if (count == 0)
            cout << " No transactions recorded for this account yet.\n";
        else
            cout << " ================================================================\n"
                 << " Total transactions: " << count << "\n";
    }

    // ─────────────────────────────────────────────────────────
    //  update()
    //  PURPOSE : Modify personal info of a logged-in account.
    //  HOW     :
    //    Sub-menu lets user choose which field to update:
    //    name / phone (re-validated) / email / Aadhaar / password.
    //    Account fields → saveaccount(); password → savepinfo().
    // ─────────────────────────────────────────────────────────
    void update() {
        cout << "\n === UPDATE ACCOUNT INFO ===\n";
        int no; account a;
        if (!login(no, a)) return;

        cout << "\n What would you like to update?\n";
        cout << " 1. Name\n 2. Phone number\n 3. Email\n"
             << " 4. Aadhaar number\n 5. Password\n 0. Cancel\n";
        cout << " Choice : ";
        int ch; cin >> ch;
        clearinput();

        switch (ch) {
            case 0: cout << "\n Update cancelled.\n"; return;
            case 1:
                cout << " New name : "; cin.getline(a.name, 50);
                saveaccount(a); break;
            case 2:
                do {
                    cout << " New phone number : "; cin >> a.phone;
                    if (!validphone(a.phone)) cout << " Invalid. Must be 10 digits.\n";
                } while (!validphone(a.phone));
                clearinput(); saveaccount(a); break;
            case 3:
                cout << " New email : "; cin.getline(a.email, 50);
                saveaccount(a); break;
            case 4:
                cout << " New Aadhaar number : "; cin.getline(a.aadhar, 20);
                saveaccount(a); break;
            case 5: {
                pinfo c; getpinfo(no, c);
                char p1[50], p2[50];
                cout << " New password     : "; cin >> p1;
                cout << " Confirm password : "; cin >> p2;
                if (strcmp(p1, p2) != 0) {
                    cout << "\n Passwords do not match. No changes made.\n"; return;
                }
                strcpy(c.password, p1); savepinfo(c); break;
            }
            default: cout << "\n Invalid choice.\n"; return;
        }
        cout << "\n Information updated successfully.\n";
    }

    // ─────────────────────────────────────────────────────────
    //  closeaccount()
    //  PURPOSE : Deactivate an account (soft delete).
    //  HOW     : Sets account.active = 0 after confirmation.
    //            The record stays in the file; login() will
    //            refuse access to inactive accounts.
    // ─────────────────────────────────────────────────────────
    void closeaccount() {
        cout << "\n === CLOSE ACCOUNT ===\n";
        int no; account a;
        if (!login(no, a)) return;

        char ans[5];
        cout << "\n Type 'yes' to confirm closing account " << a.no << " : ";
        cin >> ans;
        if (strcmp(ans, "yes") != 0) {
            cout << "\n Cancelled. Account is still active.\n"; return;
        }
        a.active = 0;
        saveaccount(a);
        cout << "\n Account closed successfully. Goodbye, " << a.name << "!\n";
    }

    // ─────────────────────────────────────────────────────────
    //  containsci(haystack, needle)
    //  PURPOSE : Case-insensitive substring search helper used
    //            by searchaccount() to match partial name/phone/
    //            email/Aadhaar strings.
    //  HOW     : Converts both strings to lowercase copies on the
    //            stack, then uses strstr() to check containment.
    //  RETURNS : true if needle appears anywhere inside haystack
    //            (case-insensitive), false otherwise.
    // ─────────────────────────────────────────────────────────
    bool containsci(const char* haystack, const char* needle) {
        // Build lowercase copies so comparison is case-insensitive
        char h[200], n[200];
        int i = 0;
        for (; haystack[i] && i < 199; i++) h[i] = tolower((unsigned char)haystack[i]);
        h[i] = '\0';
        int j = 0;
        for (; needle[j] && j < 199; j++) n[j] = tolower((unsigned char)needle[j]);
        n[j] = '\0';
        return strstr(h, n) != nullptr;
    }

    // ─────────────────────────────────────────────────────────
    //  searchaccount()
    //  PURPOSE : Let anyone look up accounts by account number,
    //            name, phone, email, or Aadhaar — without needing
    //            a password.  Only non-sensitive fields are shown
    //            (balance is hidden for privacy).
    //  HOW     :
    //    1. Show a sub-menu so the user picks the search field.
    //    2. Read the search keyword from the user.
    //    3. Scan every record in accounts.dat.
    //       - For account-number search: exact integer match.
    //       - For all other fields: case-insensitive partial match
    //         using containsci().
    //    4. Print every matching account (active OR closed).
    //    5. Report total matches found at the end.
    //  NOTE: Balance is intentionally NOT displayed here because
    //        this feature does not require authentication.
    // ─────────────────────────────────────────────────────────
    void searchaccount() {
        cout << "\n === SEARCH ACCOUNTS ===\n";
        cout << "\n Search by:\n";
        cout << "  1. Account number (exact)\n";
        cout << "  2. Name           (partial, case-insensitive)\n";
        cout << "  3. Phone number   (partial)\n";
        cout << "  4. Email address  (partial, case-insensitive)\n";
        cout << "  5. Aadhaar number (partial)\n";
        cout << "  0. Cancel\n";
        cout << "\n  Choice : ";
        int ch; cin >> ch;
        if (ch == 0) { cout << "\n Search cancelled.\n"; return; }
        if (ch < 1 || ch > 5) { cout << "\n Invalid choice.\n"; return; }

        // For account-number search we read an integer; for all others a string
        int   searchNo = 0;
        char  keyword[100] = {};

        if (ch == 1) {
            cout << "  Account number : ";
            while (!(cin >> searchNo)) {
                cout << "  Please enter a valid number: ";
                clearinput();
            }
        } else {
            clearinput();   // flush '\n' before getline
            cout << "  Search keyword : ";
            cin.getline(keyword, sizeof(keyword));
            if (strlen(keyword) == 0) {
                cout << "\n Search keyword cannot be empty.\n"; return;
            }
        }

        ifstream f(acf, ios::binary);
        if (!f) { cout << "\n No accounts found (file does not exist).\n"; return; }

        account a;
        int found = 0;
        cout << "\n Search results:\n";

        // Scan every record in the file
        while (f.read((char*)&a, sizeof(a))) {
            bool match = false;

            switch (ch) {
                case 1: match = (a.no == searchNo);              break;
                case 2: match = containsci(a.name,   keyword);  break;
                case 3: match = containsci(a.phone,  keyword);  break;
                case 4: match = containsci(a.email,  keyword);  break;
                case 5: match = containsci(a.aadhar, keyword);  break;
            }

            if (match) {
                found++;
                // Show all details EXCEPT balance (unauthenticated search)
                cout << "\n [Match " << found << "]\n";
                cout << "  Account number : " << a.no    << "\n";
                cout << "  Name           : " << a.name  << "\n";
                cout << "  Phone          : " << a.phone << "\n";
                cout << "  Email          : " << a.email << "\n";
                cout << "  Aadhaar        : " << a.aadhar << "\n";
                cout << "  Status         : " << (a.active ? "Active" : "Closed") << "\n";
                cout << "  ----------------------------------------\n";
            }
        }

        if (found == 0)
            cout << "\n No accounts matched your search.\n";
        else
            cout << "\n Found " << found << " matching account(s).\n";
    }

    // ─────────────────────────────────────────────────────────
    //  admin()
    //  PURPOSE : Show all active accounts and bank-wide totals.
    //  HOW     : Requires the hardcoded admin password "admin123".
    //            Reads every record from accounts.dat, skips
    //            closed accounts, prints active ones and sums
    //            up total balance held in the bank.
    // ─────────────────────────────────────────────────────────
    void admin() {
        cout << "\n === ADMIN PANEL ===\n";
        char p[50];
        cout << " Enter admin password : "; cin >> p;
        if (strcmp(p, "admin123") != 0) {
            cout << "\n Wrong password. Access denied.\n"; return;
        }

        ifstream f(acf, ios::binary);
        if (!f) { cout << "\n No accounts exist yet.\n"; return; }

        account a;
        int total = 0, active = 0;
        double totalbal = 0;
        cout << "\n Listing all active accounts:\n";
        while (f.read((char*)&a, sizeof(a))) {
            total++;
            if (a.active) { active++; totalbal += a.balance; showaccount(a); }
        }
        cout << "\n Total accounts : " << total
             << "  |  Active : " << active
             << "  |  Total balance in bank : Rs." << totalbal << "\n";
    }

    // ─────────────────────────────────────────────────────────
    //  run()
    //  PURPOSE : Main event loop — show menu and dispatch calls.
    //  HOW     : Infinite while(true) loop. Prints the menu,
    //            reads the user's choice, calls the matching
    //            function. Choice 0 exits. After each operation
    //            the user presses Enter to see the menu again.
    // ─────────────────────────────────────────────────────────
    void run() {
        int ch;
        while (true) {
            cout << "\n ╔════════════════════════════╗\n";
            cout <<   " ║       BANK SYSTEM          ║\n";
            cout <<   " ╠════════════════════════════╣\n";
            cout <<   " ║  1. Create account         ║\n";
            cout <<   " ║  2. Deposit money          ║\n";
            cout <<   " ║  3. Withdraw money         ║\n";
            cout <<   " ║  4. Account details        ║\n";
            cout <<   " ║  5. Transaction history    ║\n";
            cout <<   " ║  6. Search accounts        ║\n";
            cout <<   " ║  7. Update info            ║\n";
            cout <<   " ║  8. Close account          ║\n";
            cout <<   " ║  9. Admin panel            ║\n";
            cout <<   " ║  0. Exit                   ║\n";
            cout <<   " ╚════════════════════════════╝\n";
            cout <<   " Enter choice : ";
            cin  >> ch;

            switch (ch) {
                case 1: createaccount();  break;
                case 2: deposit();        break;
                case 3: withdraw();       break;
                case 4: checkbalance();   break;
                case 5: history();        break;
                case 6: searchaccount();  break;
                case 7: update();         break;
                case 8: closeaccount();   break;
                case 9: admin();          break;
                case 0:
                    cout << "\n Thank you for using our bank. Goodbye!\n";
                    return;
                default:
                    cout << "\n Invalid choice. Please enter 0-9.\n";
            }

            cout << "\n Press Enter to return to main menu...";
            clearinput();
            cin.get();
        }
    }
};

// ─────────────────────────────────────────────────────────────
//  main()
//  Entry point. Creates a bank object and starts the menu loop.
// ─────────────────────────────────────────────────────────────
int main() {
    bank b;
    b.run();
    return 0;
}
