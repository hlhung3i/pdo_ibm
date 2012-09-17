-- To initialize db2:
--   sudo -i -u db2inst1 db2 -tvsf `pwd`/initialize.ibm.sql

-- DROP DATABASE AL32UTF8;
-- CREATE DATABASE AL32UTF8;

/**
 * To login sqlplus:
 *   sqlplus SYS@AL32UTF8/CHANGE AS SYSDBA @drupal_web_test_case.oci.sql
 */

-- Connect with SYS and create new user "test".
-- [ORA] CONNECT SYS@AL32UTF8/CHANGE AS SYSDBA
-- [ORA] 
-- [ORA] DROP USER test CASCADE;
-- [ORA] 
-- [ORA] CREATE USER test PROFILE "DEFAULT" IDENTIFIED BY "CHANGE" DEFAULT TABLESPACE "USERS" TEMPORARY TABLESPACE "TEMP" ACCOUNT UNLOCK;
-- [ORA] GRANT "AQ_ADMINISTRATOR_ROLE" TO test;
-- [ORA] GRANT "AQ_USER_ROLE" TO test;
-- [ORA] GRANT "AUTHENTICATEDUSER" TO test;
-- [ORA] GRANT "CONNECT" TO test;
-- [ORA] GRANT "CTXAPP" TO test;
-- [ORA] GRANT "DBA" TO test;
-- [ORA] GRANT "DELETE_CATALOG_ROLE" TO test;
-- [ORA] GRANT "EJBCLIENT" TO test;
-- [ORA] GRANT "EXECUTE_CATALOG_ROLE" TO test;
-- [ORA] GRANT "EXP_FULL_DATABASE" TO test;
-- [ORA] GRANT "GATHER_SYSTEM_STATISTICS" TO test;
-- [ORA] 
-- [ORA] -- Connect as user "test".
-- [ORA] CONNECT test@AL32UTF8/CHANGE
-- [ORA] 
-- [ORA] EXIT;

-- 1. Connect to database and Drop all objects (and the user itself) owned by test user
-- 2. Re-Connect with test user
-- x. Quit the program


-- 1.
CONNECT TO AL32UTF8 USER test USING CHANGE

-- 2. Drop the test* tables, sequences and triggers
Drop Trigger test_id_trg
Drop Sequence test_id_seq
Drop Table test

Drop Table test_people

Drop Trigger test_one_blob_id_trg
Drop Sequence test_one_blob_id_seq
Drop Table test_one_blob

Drop Trigger test_two_blob_id_trg
Drop Sequence test_two_blobs_id_seq
Drop Table test_two_blobs

Drop Trigger test_task_id_trg
Drop Sequence test_task_id_seq
Drop Table test_task

Drop Trigger test_null_id_trg
Drop Sequence test_null_id_seq
Drop Table test_null

Drop Trigger test_serialized_id_trg
Drop Sequence test_serialized_id_seq
Drop Table test_serialized

-- 3. Re-Create the test* tables, sequences and triggers
Create Table test (id integer not null primary key, name varchar(255) default '' not null, age integer default 0 not null, job varchar(255) default 'Undefined' not null)
Alter Table test add unique (name)
Create Index test_age_idx on test (age)
Create Sequence test_id_seq
Create Trigger test_id_trg NO CASCADE BEFORE INSERT ON test REFERENCING NEW AS n FOR EACH ROW SET n.id = NEXTVAL FOR test_id_seq


Create Table test_people (name varchar(255) default '' not null, age integer default 0 not null, job varchar(255) default '' not null primary key)
Create Index test_people_age_idx on test_people (age)

Create Table test_one_blob (id integer not null primary key, blob1 blob)
Create Sequence test_one_blob_id_seq
Create Trigger test_one_blob_id_trg NO CASCADE BEFORE INSERT ON test_one_blob REFERENCING NEW AS n FOR EACH ROW SET n.id = NEXTVAL FOR test_one_blob_id_seq

Create Table test_two_blobs (id integer not null primary key, blob1 blob, blob2 blob)
Create Sequence test_two_blobs_id_seq
Create Trigger test_two_blobs_id_trg NO CASCADE BEFORE INSERT ON test_two_blobs REFERENCING NEW AS n FOR EACH ROW SET n.id = NEXTVAL FOR test_two_blobs_id_seq

Create Table test_task (tid integer not null primary key, pid integer default 0 not null, task varchar(255) default '' not null, priority integer default 0 not null)
Create Sequence test_task_id_seq
Create Trigger test_task_id_trg NO CASCADE BEFORE INSERT ON test_task REFERENCING NEW AS n FOR EACH ROW SET n.tid = NEXTVAL FOR test_task_id_seq

Create Table test_null (id integer not null primary key, name varchar(255) default '' NOT NULL, age integer default 0 not null)
Alter Table test_null add unique (name) -- failed for unique key not allow NULL
Create Index test_null_ages_idx on test_null (age)
Create Sequence test_null_id_seq
Create Trigger test_null_id_trg NO CASCADE BEFORE INSERT ON test_null REFERENCING NEW AS n FOR EACH ROW SET n.id = NEXTVAL FOR test_null_id_seq

Create Table test_serialized (id integer not null primary key, name varchar(255) default '' NOT NULL, info blob)
Alter Table test_serialized add unique (name) -- failed for unique key not allow NULL
Create Sequence test_serialized_id_seq
Create Trigger test_serialized_id_trg NO CASCADE BEFORE INSERT ON test_serialized REFERENCING NEW AS n FOR EACH ROW SET n.id = NEXTVAL FOR test_serialized_id_seq


-- x. Quit the database connection
QUIT
