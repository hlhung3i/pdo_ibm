<?php

/**
 * @file
 * Installation code for DB2 embedded database engine.
 */

/**
 * Specifies installation tasks for DB2 and equivalent databases.
 */
class DatabaseTasks_ibm extends DatabaseTasks {
  /**
   * The PDO driver name for DB2 and equivalent databases.
   *
   * @var string
   */
  protected $pdoDriver = 'ibm';

  public function __construct() {
    $this->tasks[] = array(
      'function' => 'initializeDatabase',
      'arguments' => array(),
    );
  }

  /**
   * Returns a human-readable name string for DB2 and equivalent databases.
   */
  public function name() {
    return st('DB2');
  }

  /**
   * Returns the minimum version for DB2.
   */
  public function minimumVersion() {
    return '10.1';
  }

  /**
   * Make DB2 Drupal friendly.
   *
   *   substring: DB2 native
   *   substring_index: implemented
   *   rand: DB2 native
   *
   */
  public function initializeDatabase() {
    try {
//      db_query('CREATE OR REPLACE
//        FUNCTION substring
//          (
//            p_s NVARCHAR2,
//            p_start NUMBER,
//            p_length NUMBER DEFAULT NULL)
//          RETURN NVARCHAR2
//        AS
//        BEGIN
//          IF p_length IS NULL THEN
//            RETURN SUBSTR(p_s, p_start);
//          ELSE
//            RETURN SUBSTR(p_s, p_start, p_length);
//          END IF;
//        END;');
      db_query('CREATE OR REPLACE
        FUNCTION substring_index(IN VARCHAR(2000),DELIMIT VARCHAR(200), N INT)
          RETURNS VARCHAR(2000)
          DETERMINISTIC NO EXTERNAL ACTION CONTAINS SQL 
          BEGIN ATOMIC 
            DECLARE OUT VARCHAR(2000);
            DECLARE DEM VARCHAR(2000);
            DECLARE NUM INT;
            DECLARE POS INT;
            DECLARE TEMP VARCHAR(2000);
            SET DEM=DELIMIT;
            SET TEMP=IN;
            SET NUM=N;
            SET POS=1;
            IF(NUM<0) THEN
              WHILE(LOCATE(DELIMIT,TEMP)!=0) DO
                SET TEMP=SUBSTR(TEMP,LOCATE(DELIMIT,TEMP)+1);
                SET NUM=NUM+1;
              END WHILE;
              SET NUM=NUM+1;
              SET TEMP=IN; 
            END IF;
            WHILE (NUM>0) DO
              SET POS=POS+LOCATE(DELIMIT,TEMP)-1;
              SET TEMP=SUBSTR(TEMP,LOCATE(DELIMIT,TEMP)+1);
              SET NUM=NUM-1;
            END WHILE; 
            IF(N>0) THEN 
              RETURN SUBSTR(IN,1,POS); 
            ELSE
              RETURN SUBSTR(IN,POS+1);
          END IF;
        END');
//      db_query('CREATE OR REPLACE
//        FUNCTION rand
//          RETURN NUMBER
//        AS
//        BEGIN
//          RETURN ABS(MOD(DBMS_RANDOM.RANDOM,10000001)/10000000);
//        END;');
    }
    catch (Exception $e) {
      $this->fail(st('Drupal could not be correctly setup with the existing database. Revise any errors.'));
    }
  }
}
