/*
 * Wazuh DBSYNC
 * Copyright (C) 2015-2020, Wazuh Inc.
 * July 11, 2020.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include <iostream>
#include "dbsync_test.h"
#include "dbsync.h"

constexpr auto DATABASE_TEMP {"TEMP.db"};

struct smartDeleterJson
{
    void operator()(cJSON * data)
    {
        cJSON_Delete(data);
    }
};

struct CharDeleter
{
    void operator()(char* json)
    {
        cJSON_free(json);
    }
};

static void logFunction(const char* msg)
{
    if (msg)
    {
        std::cout << msg << std::endl;
    }
}

void DBSyncTest::SetUp()
{
    dbsync_initialize(&logFunction);
};

void DBSyncTest::TearDown()
{
    EXPECT_NO_THROW(dbsync_teardown());
};

TEST_F(DBSyncTest, Initialization)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);
}

TEST_F(DBSyncTest, InitializationNullptr)
{
    const auto handle_1 { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, nullptr) };
    ASSERT_EQ(nullptr, handle_1);
    const auto handle_2 { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, nullptr, "valid") };
    ASSERT_EQ(nullptr, handle_2);
}

TEST_F(DBSyncTest, InsertData)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}]})"};
    
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };

    EXPECT_EQ(0, dbsync_insert_data(handle, jsInsert.get()));
}

TEST_F(DBSyncTest, InsertDataNullptr)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
   
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    EXPECT_NE(0, dbsync_insert_data(handle, nullptr));
}

TEST_F(DBSyncTest, InsertDataInvalidHandle)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}]})"};
    
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };

    EXPECT_NE(0, dbsync_insert_data(reinterpret_cast<void *>(0xffffffff), jsInsert.get()));
}

TEST_F(DBSyncTest, UpdateData)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}]})"};
    
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };

    cJSON * json_response { nullptr };
    
    EXPECT_EQ(0, dbsync_update_with_snapshot(handle, jsInsert.get(), &json_response));
    EXPECT_NE(nullptr, json_response);
    EXPECT_NO_THROW(dbsync_free_result(&json_response));
}

TEST_F(DBSyncTest, FreeNullptrResult)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    cJSON* json_response { nullptr };

    EXPECT_NO_THROW(dbsync_free_result(&json_response));
}

TEST_F(DBSyncTest, UpdateDataWithLessFields)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT,`path` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}]})"};
    
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };

    cJSON * json_response { nullptr };
    
    EXPECT_EQ(0, dbsync_update_with_snapshot(handle, jsInsert.get(), &json_response));
    EXPECT_NE(nullptr, json_response);
    EXPECT_NO_THROW(dbsync_free_result(&json_response));
}

TEST_F(DBSyncTest, SetMaxRows)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);
    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 100));
    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 0));
}

TEST_F(DBSyncTest, TryToInsertMoreThanMaxRows)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}, {"pid":3,"name":"cmd"}]})"};

    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };

    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 1));
    EXPECT_NE(0, dbsync_insert_data(handle, jsInsert.get()));

    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 0));
    EXPECT_EQ(0, dbsync_insert_data(handle, jsInsert.get()));
}

TEST_F(DBSyncTest, TryToUpdateMaxRowsElements)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}, {"pid":3,"name":"cmd"}]})"};
    const auto updateSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"Cmd"}, {"pid":3,"name":"System"}]})"};

    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 2));

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };
    EXPECT_EQ(0, dbsync_insert_data(handle, jsInsert.get()));

    cJSON * json_response { nullptr };
    const std::unique_ptr<cJSON, smartDeleterJson> jsUpdate{ cJSON_Parse(updateSqlStmt) };
    EXPECT_EQ(0, dbsync_update_with_snapshot(handle, jsUpdate.get(), &json_response));
    EXPECT_NE(nullptr, json_response);
    EXPECT_NO_THROW(dbsync_free_result(&json_response));
}

TEST_F(DBSyncTest, TryToUpdateMoreThanMaxRowsElements)
{
    const auto sql{ "CREATE TABLE processes(`pid` BIGINT, `name` TEXT, PRIMARY KEY (`pid`)) WITHOUT ROWID;"};
    const auto insertionSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"System"}, {"pid":3,"name":"cmd"}]})"};
    const auto updateSqlStmt{ R"({"table":"processes","data":[{"pid":4,"name":"Cmd"}, {"pid":3,"name":"System"}, {"pid":5,"name":"powershell"}]})"};

    const auto handle { dbsync_create(HostType::AGENT, DbEngineType::SQLITE3, DATABASE_TEMP, sql) };
    ASSERT_NE(nullptr, handle);

    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 2));

    const std::unique_ptr<cJSON, smartDeleterJson> jsInsert{ cJSON_Parse(insertionSqlStmt) };
    EXPECT_EQ(0, dbsync_insert_data(handle, jsInsert.get()));

    cJSON * json_response { nullptr };
    const std::unique_ptr<cJSON, smartDeleterJson> jsUpdate{ cJSON_Parse(updateSqlStmt) };
    EXPECT_NE(0, dbsync_update_with_snapshot(handle, jsUpdate.get(), &json_response));
    EXPECT_EQ(nullptr, json_response);

    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 0));
    EXPECT_EQ(0, dbsync_set_table_max_rows(handle, "processes", 10));
    EXPECT_EQ(0, dbsync_update_with_snapshot(handle, jsUpdate.get(), &json_response));
    EXPECT_NE(nullptr, json_response);
    EXPECT_NO_THROW(dbsync_free_result(&json_response));
}