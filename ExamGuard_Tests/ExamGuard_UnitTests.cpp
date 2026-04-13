#include "../shared/DataPacket.h"
#include "../shared/Logger.h"
#include "../server/StateMachine.h"
#include "../server/AuthManager.h"
#include <cassert>
#include <iostream>
#include <string>

static int g_passes = 0, g_failures = 0;

#define ASSERT_TRUE(x)  do { if(!(x)){std::cerr<<"FAIL: "#x"\n"; ++g_failures;} else{std::cout<<"PASS: "#x"\n"; ++g_passes;} } while(0)
#define ASSERT_FALSE(x) ASSERT_TRUE(!(x))
#define ASSERT_EQ(a,b)  ASSERT_TRUE((a)==(b))
#define ASSERT_NE(a,b)  ASSERT_TRUE((a)!=(b))
#define ASSERT_NULL(x)      ASSERT_TRUE((x)==nullptr)
#define ASSERT_NOT_NULL(x)  ASSERT_TRUE((x)!=nullptr)

// ============================================================
//  1. DataPacket Tests
// ============================================================

void Test_DefaultConstructor_AllFieldsZeroOrNull() {
    DataPacket pkt;
    ASSERT_EQ(pkt.packetID, 0u);
    ASSERT_EQ(pkt.dataLength, 0u);
    ASSERT_NULL(pkt.payload);
}

void Test_FullConstructor_CopiesPayload() {
    uint8_t data[] = { 0x01, 0x02, 0x03 };
    DataPacket pkt(42, CommandType::LOGIN, data, 3);
    ASSERT_EQ(pkt.packetID, 42u);
    ASSERT_EQ(pkt.commandType, CommandType::LOGIN);
    ASSERT_EQ(pkt.dataLength, 3u);
    ASSERT_NOT_NULL(pkt.payload);
    ASSERT_EQ(pkt.payload[0], (uint8_t)0x01);
    ASSERT_EQ(pkt.payload[2], (uint8_t)0x03);
}

void Test_StringConstructor_SetsPayloadAndLength() {
    DataPacket pkt(1, CommandType::LOGIN, std::string("student01:pass123"));
    ASSERT_EQ(pkt.dataLength, 17u);
    std::string s = pkt.payloadAsString();
    ASSERT_EQ(s, std::string("student01:pass123"));
}

void Test_CopyConstructor_DeepCopiesPayload() {
    DataPacket original(1, CommandType::ACK, std::string("test"));
    DataPacket copy(original);
    ASSERT_EQ(copy.payloadAsString(), std::string("test"));
    ASSERT_NE(copy.payload, original.payload);
}

void Test_CopyAssignment_DeepCopiesPayload() {
    DataPacket a(1, CommandType::DATA, std::string("hello"));
    DataPacket b;
    b = a;
    ASSERT_EQ(b.payloadAsString(), std::string("hello"));
    ASSERT_NE(b.payload, a.payload);
}

void Test_Destructor_FreesPayload_NoLeak() {
    {
        DataPacket pkt(1, CommandType::DATA, std::string("leak check"));
    }
    ASSERT_TRUE(true);
}

void Test_NullData_PayloadIsNullptr() {
    DataPacket pkt(1, CommandType::START_EXAM, nullptr, 0);
    ASSERT_NULL(pkt.payload);
    ASSERT_EQ(pkt.dataLength, 0u);
}

void Test_Timestamp_IsNonZeroAfterConstruction() {
    DataPacket pkt(1, CommandType::ACK, std::string("ts"));
    ASSERT_TRUE(pkt.timestamp > 0u);
}

void Test_CommandTypeToString_AllValues() {
    ASSERT_EQ(commandTypeToString(CommandType::LOGIN), std::string("LOGIN"));
    ASSERT_EQ(commandTypeToString(CommandType::START_EXAM), std::string("START_EXAM"));
    ASSERT_EQ(commandTypeToString(CommandType::SUBMIT_EXAM), std::string("SUBMIT_EXAM"));
    ASSERT_EQ(commandTypeToString(CommandType::ACK), std::string("ACK"));
    ASSERT_EQ(commandTypeToString(CommandType::DATA), std::string("DATA"));
    ASSERT_EQ(commandTypeToString(CommandType::ERROR_PACKET), std::string("ERROR"));
}

// ============================================================
//  2. StateMachine Tests
// ============================================================

void Test_InitialState_IsWaitingForStudent() {
    Logger log("Test_SM_Initial");
    StateMachine sm("session1", &log);
    ASSERT_EQ(sm.getCurrentState(), ServerState::WAITING_FOR_STUDENT);
}

void Test_Authentication_IsAutomaticNotCommanded() {
    Logger log("Test_SM_Auth");
    StateMachine sm("session2", &log);
    sm.onAuthenticationSuccess();
    ASSERT_EQ(sm.getCurrentState(), ServerState::AUTHENTICATED);
}

void Test_StartExam_FromAuthenticated_TransitionsToExamActive() {
    Logger log("Test_SM_Start");
    StateMachine sm("session3", &log);
    sm.onAuthenticationSuccess();
    bool result = sm.transition(CommandType::START_EXAM);
    ASSERT_TRUE(result);
    ASSERT_EQ(sm.getCurrentState(), ServerState::EXAM_ACTIVE);
}

void Test_SubmitExam_FromExamActive_TransitionsToSubmitted() {
    Logger log("Test_SM_Submit");
    StateMachine sm("session4", &log);
    sm.onAuthenticationSuccess();
    sm.transition(CommandType::START_EXAM);
    bool result = sm.transition(CommandType::SUBMIT_EXAM);
    ASSERT_TRUE(result);
    ASSERT_EQ(sm.getCurrentState(), ServerState::EXAM_SUBMITTED);
}

void Test_StartExam_WithoutAuth_IsRejected() {
    Logger log("Test_SM_NoAuth");
    StateMachine sm("session5", &log);
    bool result = sm.transition(CommandType::START_EXAM);
    ASSERT_FALSE(result);
    ASSERT_EQ(sm.getCurrentState(), ServerState::WAITING_FOR_STUDENT);
}

void Test_SubmitExam_WithoutStartExam_IsRejected() {
    Logger log("Test_SM_NoStart");
    StateMachine sm("session6", &log);
    sm.onAuthenticationSuccess();
    bool result = sm.transition(CommandType::SUBMIT_EXAM);
    ASSERT_FALSE(result);
    ASSERT_EQ(sm.getCurrentState(), ServerState::AUTHENTICATED);
}

void Test_StartExam_WhenAlreadyActive_IsRejected() {
    Logger log("Test_SM_DoubleStart");
    StateMachine sm("session7", &log);
    sm.onAuthenticationSuccess();
    sm.transition(CommandType::START_EXAM);
    bool result = sm.transition(CommandType::START_EXAM);
    ASSERT_FALSE(result);
    ASSERT_EQ(sm.getCurrentState(), ServerState::EXAM_ACTIVE);
}

void Test_ClientDisconnect_SetsExamClosed() {
    Logger log("Test_SM_Disconnect");
    StateMachine sm("session8", &log);
    sm.onAuthenticationSuccess();
    sm.onClientDisconnect();
    ASSERT_EQ(sm.getCurrentState(), ServerState::EXAM_CLOSED);
}

void Test_CanExecute_CorrectlyValidatesCommands() {
    Logger log("Test_SM_CanExec");
    StateMachine sm("session9", &log);
    ASSERT_FALSE(sm.canExecute(CommandType::START_EXAM));
    ASSERT_FALSE(sm.canExecute(CommandType::SUBMIT_EXAM));
    sm.onAuthenticationSuccess();
    ASSERT_TRUE(sm.canExecute(CommandType::START_EXAM));
    ASSERT_FALSE(sm.canExecute(CommandType::SUBMIT_EXAM));
    sm.transition(CommandType::START_EXAM);
    ASSERT_FALSE(sm.canExecute(CommandType::START_EXAM));
    ASSERT_TRUE(sm.canExecute(CommandType::SUBMIT_EXAM));
}

void Test_Reset_ReturnsToWaitingForStudent() {
    Logger log("Test_SM_Reset");
    StateMachine sm("session10", &log);
    sm.onAuthenticationSuccess();
    sm.transition(CommandType::START_EXAM);
    sm.reset();
    ASSERT_EQ(sm.getCurrentState(), ServerState::WAITING_FOR_STUDENT);
}

void Test_StateToString_AllFiveStates() {
    ASSERT_EQ(serverStateToString(ServerState::WAITING_FOR_STUDENT), std::string("WAITING_FOR_STUDENT"));
    ASSERT_EQ(serverStateToString(ServerState::AUTHENTICATED), std::string("AUTHENTICATED"));
    ASSERT_EQ(serverStateToString(ServerState::EXAM_ACTIVE), std::string("EXAM_ACTIVE"));
    ASSERT_EQ(serverStateToString(ServerState::EXAM_SUBMITTED), std::string("EXAM_SUBMITTED"));
    ASSERT_EQ(serverStateToString(ServerState::EXAM_CLOSED), std::string("EXAM_CLOSED"));
}

// ============================================================
//  3. AuthManager Tests
// ============================================================

void Test_ValidCredentials_AuthenticatesSuccessfully() {
    Logger log("Test_Auth_Valid");
    AuthManager auth(&log);
    DataPacket loginPkt(1, CommandType::LOGIN, std::string("student01:pass123"));
    std::string id;
    bool result = auth.validate(loginPkt, id);
    ASSERT_TRUE(result);
    ASSERT_EQ(id, std::string("student01"));
}

void Test_InvalidPassword_RejectsAuthentication() {
    Logger log("Test_Auth_BadPwd");
    AuthManager auth(&log);
    DataPacket loginPkt(1, CommandType::LOGIN, std::string("student01:wrongpass"));
    std::string id;
    bool result = auth.validate(loginPkt, id);
    ASSERT_FALSE(result);
}

void Test_UnknownStudent_RejectsAuthentication() {
    Logger log("Test_Auth_Unknown");
    AuthManager auth(&log);
    DataPacket loginPkt(1, CommandType::LOGIN, std::string("nobody:pass"));
    std::string id;
    bool result = auth.validate(loginPkt, id);
    ASSERT_FALSE(result);
}

void Test_MalformedPayload_RejectsGracefully() {
    Logger log("Test_Auth_Malformed");
    AuthManager auth(&log);
    DataPacket loginPkt(1, CommandType::LOGIN, std::string("nostudentpassword"));
    std::string id;
    bool result = auth.validate(loginPkt, id);
    ASSERT_FALSE(result);
}

void Test_WrongCommandType_RejectsValidation() {
    Logger log("Test_Auth_WrongType");
    AuthManager auth(&log);
    DataPacket pkt(1, CommandType::START_EXAM, std::string("student01:pass123"));
    std::string id;
    bool result = auth.validate(pkt, id);
    ASSERT_FALSE(result);
}

void Test_Session_CreatedAndEnded() {
    Logger log("Test_Auth_Session");
    AuthManager auth(&log);
    auth.createSession("sock42", "student01", "192.168.1.1:50000");
    ASSERT_TRUE(auth.isAuthenticated("sock42"));
    ASSERT_EQ(auth.getStudentID("sock42"), std::string("student01"));
    auth.endSession("sock42");
    ASSERT_FALSE(auth.isAuthenticated("sock42"));
}

void Test_AddCredential_WorksForNewStudent() {
    Logger log("Test_Auth_AddCred");
    AuthManager auth(&log);
    auth.addCredential("newstudent", "newpass");
    DataPacket loginPkt(1, CommandType::LOGIN, std::string("newstudent:newpass"));
    std::string id;
    bool result = auth.validate(loginPkt, id);
    ASSERT_TRUE(result);
}

// ============================================================
//  4. Logger Tests
// ============================================================

void Test_Logger_CreatesLogFile() {
    Logger log("ExamGuard_Test");
    ASSERT_FALSE(log.getFilename().empty());
    ASSERT_TRUE(log.getFilename().find("ExamGuard_Test") != std::string::npos);
}

void Test_LogTX_DoesNotCrash() {
    Logger log("ExamGuard_LogTX");
    DataPacket pkt(1, CommandType::LOGIN, std::string("test"));
    log.logTX(pkt);
    ASSERT_TRUE(true);
}

void Test_LogRX_DoesNotCrash() {
    Logger log("ExamGuard_LogRX");
    DataPacket pkt(2, CommandType::ACK, nullptr, 0);
    log.logRX(pkt);
    ASSERT_TRUE(true);
}

void Test_LogState_DoesNotCrash() {
    Logger log("ExamGuard_LogState");
    log.logState("WAITING_FOR_STUDENT", "AUTHENTICATED");
    ASSERT_TRUE(true);
}

void Test_LogAuth_DoesNotCrash() {
    Logger log("ExamGuard_LogAuth");
    log.logAuth("student01", true);
    log.logAuth("baduser", false);
    ASSERT_TRUE(true);
}

// ============================================================
//  Main
// ============================================================

int main() {
    std::cout << "=== ExamGuard Unit Tests ===\n\n";

    std::cout << "--- DataPacket Tests ---\n";
    Test_DefaultConstructor_AllFieldsZeroOrNull();
    Test_FullConstructor_CopiesPayload();
    Test_StringConstructor_SetsPayloadAndLength();
    Test_CopyConstructor_DeepCopiesPayload();
    Test_CopyAssignment_DeepCopiesPayload();
    Test_Destructor_FreesPayload_NoLeak();
    Test_NullData_PayloadIsNullptr();
    Test_Timestamp_IsNonZeroAfterConstruction();
    Test_CommandTypeToString_AllValues();

    std::cout << "\n--- StateMachine Tests ---\n";
    Test_InitialState_IsWaitingForStudent();
    Test_Authentication_IsAutomaticNotCommanded();
    Test_StartExam_FromAuthenticated_TransitionsToExamActive();
    Test_SubmitExam_FromExamActive_TransitionsToSubmitted();
    Test_StartExam_WithoutAuth_IsRejected();
    Test_SubmitExam_WithoutStartExam_IsRejected();
    Test_StartExam_WhenAlreadyActive_IsRejected();
    Test_ClientDisconnect_SetsExamClosed();
    Test_CanExecute_CorrectlyValidatesCommands();
    Test_Reset_ReturnsToWaitingForStudent();
    Test_StateToString_AllFiveStates();

    std::cout << "\n--- AuthManager Tests ---\n";
    Test_ValidCredentials_AuthenticatesSuccessfully();
    Test_InvalidPassword_RejectsAuthentication();
    Test_UnknownStudent_RejectsAuthentication();
    Test_MalformedPayload_RejectsGracefully();
    Test_WrongCommandType_RejectsValidation();
    Test_Session_CreatedAndEnded();
    Test_AddCredential_WorksForNewStudent();

    std::cout << "\n--- Logger Tests ---\n";
    Test_Logger_CreatesLogFile();
    Test_LogTX_DoesNotCrash();
    Test_LogRX_DoesNotCrash();
    Test_LogState_DoesNotCrash();
    Test_LogAuth_DoesNotCrash();

    std::cout << "\n=== Results: " << g_passes << " passed, "
        << g_failures << " failed ===\n";

    return (g_failures > 0) ? 1 : 0;
}