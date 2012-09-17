<?php

/**
 * @file
 * Database schema code for DB2 database servers.
 */

/**
 * @ingroup schemaapi
 * @{
 */

class DatabaseSchema_ibm extends DatabaseSchema {

  /**
   * Maximum length of a table comment in MySQL.
   */
  const COMMENT_MAX_TABLE = 60;

  /**
   * Maximum length of a column comment in MySQL.
   */
  const COMMENT_MAX_COLUMN = 255;

  /**
   * Get information about the table and database name from the prefix.
   *
   * @return
   *   A keyed array with information about the database, table name and prefix.
   */
  protected function getPrefixInfo($table = 'default', $add_prefix = TRUE) {
    $info = array('prefix' => $this->connection->tablePrefix($table));
    if ($add_prefix) {
      $table = $info['prefix'] . $table;
    }
    if (($pos = strpos($table, '.')) !== FALSE) {
      $info['database'] = substr($table, 0, $pos);
      $info['table'] = substr($table, ++$pos);
    }
    else {
      $db_info = Database::getConnectionInfo();
      $info['database'] = $db_info['default']['database'];
      $info['table'] = $table;
    }
    return $info;
  }

  /**
   * Build a condition to match a table name against a standard information_schema.
   *
   * MySQL uses databases like schemas rather than catalogs so when we build
   * a condition to query the information_schema.tables, we set the default
   * database as the schema unless specified otherwise, and exclude table_catalog
   * from the condition criteria.
   */
  protected function buildTableNameCondition($table_name, $operator = '=', $add_prefix = TRUE) {
    $info = $this->connection->getConnectionOptions();

    $table_info = $this->getPrefixInfo($table_name, $add_prefix);

    $condition = new DatabaseCondition('AND');
    $condition->condition('table_schema', $table_info['database']);
    $condition->condition('table_name', $table_info['table'], $operator);
    return $condition;
  }

  /**
   * A cache of information about blob columns of tables.
   *
   * This is collected by DatabaseConnection_ibm->queryTableInformation(),
   * by introspecting the database.
   *
   * @see DatabaseConnection_ibm->queryTableInformation()
   * @var array
   */
  protected $tableInformation = array();

  /**
   * Fetch the list of blobs used on a table.
   *
   * We introspect the database to collect the information required by insert
   * and update queries.
   *
   * @param $table_name
   *   The non-prefixed name of the table.
   * @return
   *   An object with two member variables:
   *     - 'blob_fields' that lists all the blob fields in the table.
   */
  public function queryTableInformation($table, $rebuild = FALSE) {

    // Generate a key to reference this table's information on.
    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
		$db_user = $this->connection->getConnectionOptions()['username'];

    if (!isset($this->tableInformation[$table]['fields']) || $rebuild) {
      $this->tableInformation[$table]['fields'] = array();

      // Don't use {} around user_tab_cols table.
      // TODO: !! Need Rewrite to get Primary Key column for THIS user
      $results = $this->connection->query("SELECT COLNAME FROM SYSCAT.COLUMNS COL LEFT JOIN SYSCAT.TABLES TAB ON COL.TABNAME = TAB.TABNAME AND COL.TABSCHEMA = TAB.TABSCHEMA WHERE COL.TYPENAME = 'BLOB' AND LCASE(COL.TABNAME) = :table_name AND LCASE(TAB.OWNER) = :db_user", array(':table_name' => strtolower($table_name), ':db_user' => strtolower($db_user)));
      foreach ($results as $result) {
        $field = strtolower($this->connection->restoreForDB2($result->colname));
        $this->tableInformation[$table]['fields'][$field]['type'] = 'blob';
      }

      // Don't use {} around user_constraints and user_cons_columns table.
//      $results = $this->connection->query("SELECT COLS.TABLE_NAME, COLS.COLUMN_NAME FROM USER_CONSTRAINTS CONS, USER_CONS_COLUMNS COLS, USER_TAB_COLS TCOLS WHERE CONS.CONSTRAINT_TYPE = 'P' AND CONS.CONSTRAINT_NAME = COLS.CONSTRAINT_NAME AND CONS.OWNER = COLS.OWNER AND COLS.TABLE_NAME = TCOLS.TABLE_NAME AND COLS.COLUMN_NAME = TCOLS.COLUMN_NAME AND TCOLS.DATA_TYPE = 'NUMBER' AND LCASE(COLS.TABLE_NAME) = :table_name", array(':table_name' => strtolower($table_name)));
      // TODO: !! Need Rewrite to get Primary Key column for THIS user
      $results = $this->connection->query("SELECT COLNAME FROM SYSCAT.COLUMNS COL LEFT JOIN SYSCAT.TABLES TAB ON COL.TABNAME = TAB.TABNAME AND COL.TABSCHEMA = TAB.TABSCHEMA WHERE LCASE(COL.TABNAME) = :table_name AND COL.KEYSEQ > 0 AND LCASE(TAB.OWNER) = :db_user ORDER BY KEYSEQ ASC", array(':table_name' => strtolower($table_name), ':db_user' => strtolower($db_user)));
      foreach ($results as $result) {
        $field = strtolower($this->connection->restoreForDB2($result->colname));
        $sequence_name = $this->connection->makeSequenceName($table, $field);
        $sequence_name = $this->connection->escapeForDB2($sequence_name);

        // Don't use {} around SYSCAT.SEQUENCES table.
      // TODO: !! Need Rewrite to filter record for THIS user
        if ($this->connection->query('SELECT 1 FROM SYSCAT.SEQUENCES WHERE LCASE(SEQNAME) = :sequence_name AND LCASE(OWNER) = :sequence_owner', array(':sequence_name' => strtolower($sequence_name), ':sequence_owner' => strtolower($db_user)))->fetchField()) {
          $this->tableInformation[$table]['fields'][$field]['type'] = 'serial';
        }
      }
    }

    return $this->tableInformation[$table];
  }

  public function triggerExists($table, $field, $trg_suffix) {
    $trigger_name = $this->prefixNonTable($table, $field, 'trg');
		$db_user = $this->connection->getConnectionOptions()['username'];

    // Don't use {} around user_tables table.
    return (bool) $this->connection->query('SELECT 1 FROM SYSCAT.TRIGGERS WHERE LCASE(TRIGNAME) = :trigger_name AND LCASE(OWNER) = :db_user', array(':trigger_name' => strtolower($trigger_name), ':db_user' => strtolower($db_user)))->fetchField();
  }

  public function dropTrigger($table, $field, $trg_suffix='trg') {
    if (!$this->tableExists($table, $field, $trg_suffix)) {
      return FALSE;
    }

    // Drop Trigger
    $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
    return TRUE;
  }

  public function tableExists($table) {
    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
		$db_user = $this->connection->getConnectionOptions()['username'];

    // Don't use {} around user_tables table.
    return (bool) $this->connection->query('SELECT 1 FROM SYSCAT.TABLES WHERE LCASE(TABNAME) = :table_name AND LCASE(OWNER) = :db_user', array(':table_name' => strtolower($table_name), ':db_user' => strtolower($db_user)))->fetchField();
  }

  public function findTables($table_expression) {
		$db_user = $this->connection->getConnectionOptions()['username'];

    // We need to manually search for the table name, since fetched
    // result should be restored as original long identifier.
    $table_expression = '/^' . strtr($table_expression, '%', '.*') . '$/';

    // Don't use {} around user_tables table.
    $results = $this->connection->query('SELECT TABNAME FROM SYSCAT.TABLES WHERE LCASE(OWNER) = :db_user', array(':db_user' => strtolower($db_user)));

    $tables = array();
    foreach ($results as $result) {
      $table_name = $this->connection->restoreForDB2(strtolower($result->tabname));
      if (preg_match($table_expression, $table_name)) {
        $tables[$table_name] = $table_name;
      }
    }

    return $tables;
  }

  public function fieldExists($table, $column) {
    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $column_name = $this->connection->escapeForDB2($column);
		$db_user = $this->connection->getConnectionOptions()['username'];

    // Don't use {} around user_tab_cols table.
    return (bool) $this->connection->query('SELECT 1 FROM SYSCAT.COLUMNS COL LEFT JOIN SYSCAT.TABLES TAB ON COL.TABNAME = TAB.TABNAME AND COL.TABSCHEMA = TAB.TABSCHEMA WHERE LCASE(COL.TABNAME) = :table_name AND LCASE(COL.COLNAME) = :column_name AND LCASE(TAB.OWNER) = :db_user', array(':table_name' => strtolower($table_name), ':column_name' => strtolower($column_name), ':db_user' => strtolower($db_user)))->fetchField();
  }

  /**
   * Generate SQL to create a new table from a Drupal schema definition.
   *
   * @param $name
   *   The name of the table to create.
   * @param $table
   *   A Schema API table definition array.
   * @return
   *   An array of SQL statements to create the table.
   */
  protected function createTableSql($name, $table) {
    $table_name = $this->connection->prefixTables('{' . $name . '}');
    $table_name = $this->connection->escapeForDB2($table_name);

    $sql_fields = array();
		// For DB2, Primary key and Unique Key column reqired to be NOT NULL
		//  Force inject NOT NULL in table creation DDL
    foreach ($table['fields'] as $field_name => $field) {
      if ((isset($table['primary key']) && is_array($table['primary key']) && in_array($field_name, $table['primary key'])) || (isset($table['unique keys']) && is_array($table['unique keys']) && in_array($field_name, array_keys($table['unique keys'])))) {
        $force_not_null = TRUE;
      } else {
        $force_not_null = FALSE;
      }
      $sql_fields[] = $this->createFieldSql($field_name, $this->processField($field), $force_not_null);
    }

    $sql_keys = array();
    if (isset($table['primary key']) && is_array($table['primary key'])) {
      $sql_keys[] = 'CONSTRAINT ' . $this->prefixNonTable($name, 'pk') . ' PRIMARY KEY (' . implode(', ', $table['primary key']) . ')';
    }
    if (isset($table['unique keys']) && is_array($table['unique keys'])) {
      foreach ($table['unique keys'] as $key_name => $key) {
        $sql_keys[] = 'CONSTRAINT ' . $this->prefixNonTable($name, $key_name, 'uix') . ' UNIQUE (' . implode(', ', $key) . ')';
      }
    }

    $sql = 'CREATE TABLE {' . $name . '} ' . "(\n\t";
    $sql .= implode(",\n\t", $sql_fields);
    if (count($sql_keys) > 0) {
      $sql .= ",\n\t";
    }
    $sql .= implode(",\n\t", $sql_keys);
    $sql .= "\n)";
    $statements[] = $sql;

    if (isset($table['indexes']) && is_array($table['indexes'])) {
      foreach ($table['indexes'] as $key_name => $key) {
        $statements[] = $this->_createIndexSql($name, $key_name, $key);
      }
    }

    foreach ($table['fields'] as $field_name => $field) {
      if (isset($field['type']) && ($field['type'] == 'serial')) {
        $statements[] = $this->_createSequenceSql($name, $field_name);
        $statements[] = $this->_createTriggerSql($name, $field_name);
      }
    }

    // Add table comment.
    if (!empty($table['description'])) {
      $statements[] = $this->connection->prepare('COMMENT ON TABLE ' . $table_name . ' IS ' . $this->prepareComment($table['description']));
    }

    // Add column comments.
    foreach ($table['fields'] as $field_name => $field) {
      if (!empty($field['description'])) {
        $field_name = $this->connection->escapeForDB2($field_name);
        $statements[] = $this->connection->prepare('COMMENT ON COLUMN ' . $table_name . '.' . $field_name . ' IS ' . $this->prepareComment($field['description']));
      }
    }

    return $statements;
  }

  /**
   * Create an SQL string for a field to be used in table creation or
   * alteration.
   *
   * Before passing a field out of a schema definition into this
   * function it has to be processed by _db_process_field().
   *
   * @param $name
   *    Name of the field.
   * @param $spec
   *    The field specification, as per the schema data structure format.
   */
  protected function createFieldSql($name, $spec, $force_not_null=FALSE) {
    $sql = $name . ' ' . $spec['ibm_type'];

//    if (isset($spec['type']) && $spec['type'] == 'serial') {
//      unset($spec['not null']);
//    }

    // With poor DB2 we have a lots of limitation.
    if (isset($spec['precision']) && isset($spec['scale'])) {
      $sql .= '(' . min($spec['precision'], DB2_MAX_NUMBER_LENGTH) . ', ' . $spec['scale'] . ')';
    }
    elseif ($spec['ibm_type'] == 'VARCHAR') {
      $sql .= '(' . (isset($spec['length']) ? min($spec['length'], DB2_MAX_NVARCHAR2_LENGTH) : DB2_MAX_NVARCHAR2_LENGTH) . ')';
    }
    elseif ($spec['ibm_type'] == 'CHAR') {
      $sql .= '(' . (isset($spec['length']) ? min($spec['length'], DB2_MAX_NCHAR_LENGTH) : DB2_MAX_NCHAR_LENGTH) . ')';
    }
    elseif ($spec['ibm_type'] == 'DECIMAL') {
      $sql .= '(' . (isset($spec['length']) ? min($spec['length'], DB2_MAX_NUMBER_LENGTH) : DB2_MAX_NUMBER_LENGTH) . ')';
    }

    // DB2 require DEFAULT before NULL.
    if (isset($spec['default'])) {
      if ($spec['ibm_type'] == 'BLOB') {
        $default = 'EMPTY_BLOB()';
      }
      elseif ($spec['ibm_type'] == 'VARCHAR' || $spec['ibm_type'] == 'CHAR') {
        $default = "'" . $spec['default'] . "'";
      }
      else {
        $default = is_string($spec['default']) ? "'" . $spec['default'] . "'" : $spec['default'];
      }
      $sql .= " DEFAULT $default";
    }

    if ((isset($spec['not null']) && $spec['not null']) || $force_not_null) {
      $sql .= ' NOT NULL';
    }

    // DB2 require CHECK constraint as the last condition.
    if (!empty($spec['unsigned'])) {
      $sql .= ' CHECK (' . $name . ' >= 0)';
    }

print 'SQL : ' . $sql . '<br />';
    return $sql;
  }

  /**
   * Set database-engine specific properties for a field.
   *
   * @param $field
   *   A field description array, as specified in the schema documentation.
   */
  protected function processField($field) {

    if (!isset($field['size'])) {
      $field['size'] = 'normal';
    }

    // Set the correct database-engine specific datatype.
    // In case one is already provided, force it to lowercase.
    if (isset($field['ibm_type'])) {
      $field['ibm_type'] = drupal_strtoupper($field['ibm_type']);
    }
    else {
      $map = $this->getFieldTypeMap();
      $field['ibm_type'] = $map[$field['type'] . ':' . $field['size']];
    }

    if (isset($field['type']) && $field['type'] == 'serial') {
      unset($field['not null']);
    }

    return $field;
  }

  /**
   * This maps a generic data type in combination with its data size
   * to the engine-specific data type.
   */
  public function getFieldTypeMap() {
    // Put :normal last so it gets preserved by array_flip. This makes
    // it much easier for modules (such as schema.module) to map
    // database types back into schema types.
    // $map does not use drupal_static as its value never changes.
    static $map = array(
      'varchar:normal'  => 'VARCHAR',
      'char:normal'     => 'CHAR',

      'text:tiny'       => 'VARCHAR',
      'text:small'      => 'VARCHAR',
      'text:medium'     => 'VARCHAR',
      'text:big'        => 'VARCHAR',
      'text:normal'     => 'VARCHAR',

      'serial:tiny'     => 'INTEGER',
      'serial:small'    => 'INTEGER',
      'serial:medium'   => 'INTEGER',
      'serial:big'      => 'BIGINT',
      'serial:normal'   => 'INTEGER',

      'int:tiny'        => 'SMALLINT',
      'int:small'       => 'SMALLINT',
      'int:medium'      => 'INTEGER',
      'int:big'         => 'BIGINT',
      'int:normal'      => 'INTEGER',

      'float:tiny'      => 'FLOAT',
      'float:small'     => 'FLOAT',
      'float:medium'    => 'FLOAT',
      'float:big'       => 'DOUBLE',
      'float:normal'    => 'FLOAT',

      'numeric:normal'  => 'DECIMAL',

      'blob:big'        => 'BLOB',
      'blob:normal'     => 'BLOB',
    );
    return $map;
  }

  protected function _reorgTableSql($table) {
		$sql = "CALL SYSPROC.ADMIN_CMD('REORG TABLE {" . $table . "}')";
//		echo 'REORG after Adding Primary Key<br /> SQL = ' . $sql . '<br />'; // JUNK
		return $sql;
	}

  protected function _createKeySql($fields) {
    $return = array();
    foreach ($fields as $field) {
      if (is_array($field)) {
        $return[] = 'SUBSTR(' . $field[0] . ', 1, ' . $field[1] . ')';
      }
      else {
        $return[] = $field;
      }
    }
    return implode(', ', $return);
  }

  public function renameTable($table, $new_name) {
		$db_user = $this->connection->getConnectionOptions()['username'];

    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot rename %table to %table_new: table %table doesn't exist.", array('%table' => $table, '%table_new' => $new_name)));
    }
    if ($this->tableExists($new_name)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot rename %table to %table_new: table %table_new already exists.", array('%table' => $table, '%table_new' => $new_name)));
    }

    $prefixed_table = $this->connection->prefixTables('{' . $table . '}');
    $prefixed_new_name = $this->connection->prefixTables('{' . $new_name . '}');

    // Rename Indexes.
    $results = $this->connection->query("SELECT INDNAME FROM SYSCAT.INDEXES WHERE LCASE(TABNAME) = :table_name AND LCASE(OWNER) = :db_user", array(':table_name' => strtolower($this->connection->escapeForDB2($prefixed_table)), ':db_user' => strtolower($db_user)));
    foreach ($results as $result) {
      $prefixed_index = strtolower($this->connection->restoreForDB2($result->indname));
      $prefixed_new_index = strtr($prefixed_index, array($prefixed_table => $prefixed_new_name));
      $this->connection->query('RENAME INDEX ' . $prefixed_index . ' TO ' . $prefixed_new_index);
    }

    // Rename Table.
    $this->connection->query('RENAME TABLE ' . $prefixed_table . ' TO ' . $prefixed_new_name);

    // Drop Trigger, rename Sequence, and re-create Trigger.
    $schema = $this->queryTableInformation($table);
    foreach ($schema['fields'] as $field => $value) {
      if ($value['type'] == 'serial') {
        $this->_renameSeqTrg($table, $field, $new_name, $field);
      }
    }
  }

  public function dropTable($table) {
    if (!$this->tableExists($table)) {
      return FALSE;
    }

    // Drop Trigger and Sequence before Table.
    $schema = $this->queryTableInformation($table);
    foreach ($schema['fields'] as $field => $value) {
      if ($value['type'] == 'serial') {
        $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
        $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
      }
    }

    $this->connection->query('DROP TABLE {' . $table . '}');
    return TRUE;
  }

  public function addField($table, $field, $spec, $new_keys = array()) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add field %table.%field: table doesn't exist.", array('%field' => $field, '%table' => $table)));
    }
    if ($this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add field %table.%field: field already exists.", array('%field' => $field, '%table' => $table)));
    }

    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $field_name = $this->connection->escapeForDB2($field);

    $fixnull = FALSE;
    if (!empty($spec['not null']) && !isset($spec['default'])) {
      $fixnull = TRUE;
      $spec['not null'] = FALSE;
    }

		// DB2 Assume the new column is neither Primary Key nor Unique Key
    $query = 'ALTER TABLE ' . $table_name . ' ADD ';
    $query .= $this->createFieldSql($field, $this->processField($spec));
    $this->connection->query($query);

    // Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));

    if (isset($spec['initial'])) {
      $this->connection->query('UPDATE ' . $table_name . ' SET ' . $field_name . ' = :initial_value', array(':initial_value' => $spec['initial']));
    }

    if ($fixnull) {
      $this->connection->query('ALTER TABLE ' . $table_name . ' ALTER COLUMN ' . $field_name . ' SET NOT NULL');
      // Reorg Table after ALTER TABLE
      $this->connection->query($this->_reorgTableSql($table));
    }

    if (isset($new_keys)) {
      $this->_createKeys($table, $new_keys);
    }

    // Add Trigger and Sequence.
    if ($spec['type'] == 'serial') {
      $this->connection->query($this->_createSequenceSql($table, $field));
      $this->connection->query($this->_createTriggerSql($table, $field));
    }

    // Add column comment.
    if (!empty($spec['description'])) {
      $stmt = $this->connection->prepare('COMMENT ON COLUMN ' . $table_name . '.' . $field_name . ' IS ' . $this->prepareComment($spec['description']));
      $this->connection->query($stmt);
    }
  }

  public function dropField($table, $field) {
    if (!$this->fieldExists($table, $field)) {
      return FALSE;
    }

    // Drop Trigger and Sequence.
    try {
      $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
    }
    catch (Exception $e) {
      // No action required.
    }
    $this->connection->query('ALTER TABLE {' . $table . '} DROP COLUMN ' . $field);
    // Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
    return TRUE;
  }

  public function fieldSetDefault($table, $field, $default) {
    if (!$this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot set default value of field %table.%field: field doesn't exist.", array('%table' => $table, '%field' => $field)));
    }

    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $field_name = $this->connection->escapeForDB2($field);

    if (!isset($default)) {
      $default = 'NULL';
    }
    else {
      $default = is_string($default) ? "'$default'" : $default;
    }

    $this->connection->query('ALTER TABLE ' . $table_name . ' ALTER COLUMN ' . $field_name . ' SET DEFAULT ' . $default . '');
    // Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
  }

  public function fieldSetNoDefault($table, $field) {
    if (!$this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot remove default value of field %table.%field: field doesn't exist.", array('%table' => $table, '%field' => $field)));
    }

    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $field_name = $this->connection->escapeForDB2($field);

    $this->connection->query('ALTER TABLE ' . $table_name . ' ALTER COLUMN ' . $field_name . ' SET DEFAULT NULL');
    // Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
  }

  public function indexExists($table, $name) {
    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $index_name = $this->connection->escapeForDB2($this->prefixNonTable($table, $name, 'ix'));
		$db_user = $this->connection->getConnectionOptions()['username'];

    // Don't use {} around user_indexes table.
    return (bool) $this->connection->query('SELECT 1 FROM SYSCAT.INDEXES WHERE LCASE(TABNAME) = :table_name AND LCASE(INDNAME) = :index_name AND LCASE(OWNER) = :db_user', array(':table_name' => strtolower($table_name), ':index_name' => strtolower($index_name), ':db_user' => strtolower($db_user)))->fetchField();
  }

  public function constraintExists($table) {
    $args = func_get_args();

    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $constraint_name = $this->connection->escapeForDB2(implode('_', $args));
		$db_user = $this->connection->getConnectionOptions()['username'];

    // Don't use {} around user_cons_columns table.
    return (bool) $this->connection->query('SELECT 1 FROM SYSCAT.TABCONST WHERE LCASE(TABNAME) = :table_name AND LCASE(CONSTNAME) = :constraint_name AND LCASE(OWNER) = :db_user', array(':table_name' => strtolower($table_name), ':constraint_name' => strtolower($constraint_name), ':db_user' => strtolower($db_user)))->fetchField();
  }

  public function addPrimaryKey($table, $fields) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add primary key to table %table: table doesn't exist.", array('%table' => $table)));
    }
    if ($this->constraintExists($table, 'pk')) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add primary key to table %table: primary key already exists.", array('%table' => $table)));
    }

    $this->connection->query('ALTER TABLE {' . $table . '} ADD CONSTRAINT ' . $this->prefixNonTable($table, 'pk') . ' PRIMARY KEY (' . implode(', ', $fields) . ')');
		// Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
  }

  public function dropPrimaryKey($table) {
    if (!$this->constraintExists($table, 'pk')) {
      return FALSE;
    }

    $this->connection->query('ALTER TABLE {' . $table . '} DROP CONSTRAINT ' . $this->prefixNonTable($table, 'pk'));
		// Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
    return TRUE;
  }

  public function addUniqueKey($table, $name, $fields) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add unique key %name to table %table: table doesn't exist.", array('%table' => $table, '%name' => $name)));
    }
    if ($this->constraintExists($table, $name, 'uix')) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add unique key %name to table %table: unique key already exists.", array('%table' => $table, '%name' => $name)));
    }

    $this->connection->query('ALTER TABLE {' . $table . '} ADD CONSTRAINT ' . $this->prefixNonTable($table, $name, 'uix') . ' UNIQUE (' . implode(', ', $fields) . ')');
		// Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
  }

  public function dropUniqueKey($table, $name) {
    if (!$this->constraintExists($table, $name, 'uix')) {
      return FALSE;
    }

    $this->connection->query('ALTER TABLE {' . $table . '} DROP CONSTRAINT ' . $this->prefixNonTable($table, $name, 'uix'));
		// Reorg Table after ALTER TABLE
    $this->connection->query($this->_reorgTableSql($table));
    return TRUE;
  }

  public function addIndex($table, $name, $fields) {
    if (!$this->tableExists($table)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot add index %name to table %table: table doesn't exist.", array('%table' => $table, '%name' => $name)));
    }
    if ($this->indexExists($table, $name)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot add index %name to table %table: index already exists.", array('%table' => $table, '%name' => $name)));
    }

    $this->connection->query($this->_createIndexSql($table, $name, $fields));
  }

  public function dropIndex($table, $name) {
    if (!$this->indexExists($table, $name)) {
      return FALSE;
    }

    $this->connection->query('DROP INDEX ' . $this->prefixNonTable($table, $name, 'ix'));
    return TRUE;
  }

  public function changeField($table, $field, $field_new, $spec, $new_keys = array()) {
    if (!$this->fieldExists($table, $field)) {
      throw new DatabaseSchemaObjectDoesNotExistException(t("Cannot change the definition of field %table.%name: field doesn't exist.", array('%table' => $table, '%name' => $field)));
    }
    if (($field != $field_new) && $this->fieldExists($table, $field_new)) {
      throw new DatabaseSchemaObjectExistsException(t("Cannot rename field %table.%name to %name_new: target field already exists.", array('%table' => $table, '%name' => $field, '%name_new' => $field_new)));
    }

    $not_null = isset($spec['not null']) ? $spec['not null'] : FALSE;
    unset($spec['not null']);

    $spec['size'] = isset($spec['size']) ? $spec['size'] : 'normal';

    // Drop Trigger and rename Sequence.
    $serial_value = 0;
    try {
      $serial_value = $this->connection->query('SELECT PREVVAL FOR ' . $this->connection->makeSequenceName($table, $field) . ' FROM SYSIBM.SYSDUMMY1')->fetchField();
      $this->connection->query('DROP TRIGGER ' . $this->prefixNonTable($table, $field, 'trg'));
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
    }
    catch (Exception $e) {
      // No action required.
    }

    // Backup legacy field data.
    $this->connection->query('ALTER TABLE {' . $table . '} RENAME COLUMN ' . $field . ' TO ' . $field . '_old');

    // Add new field and transfer legacy data.
    $this->addField($table, $field_new, $spec);
    $this->connection->query('UPDATE {' . $table . '} SET ' . $field_new . ' = ' . $field . '_old');

    if ($not_null) {
      $this->connection->query('ALTER TABLE {' . $table . '} ALTER COLUMN ' . $field_new . ' SET NOT NULL');
      // Reorg Table after ALTER TABLE
      $this->connection->query($this->_reorgTableSql($table));
    }

    $this->dropField($table, $field . '_old');

    if (isset($new_keys)) {
      $this->_createKeys($table, $new_keys);
    }

    // Restore latest Sequence ID.
    if ($serial_value > 0) {
      $this->connection->query($this->_createSequenceSql($table, $field_new, $serial_value));
      $this->connection->query($this->_createTriggerSql($table, $field_new));
    }
  }

  protected function _createIndexSql($table, $name, $fields) {
    $query = 'CREATE INDEX ' . $this->prefixNonTable($table, $name, 'ix') . ' ON {' . $table . '} (';
    $query .= $this->_createKeySql($fields) . ')';
    return $query;
  }

  protected function _createSequenceSql($table, $field, $start = 1) {
    return 'CREATE SEQUENCE ' . $this->connection->makeSequenceName($table, $field) . ' MINVALUE 1 INCREMENT BY 1 START WITH ' . $start . ' NOCACHE NOORDER NOCYCLE';
  }

  protected function _renameSeqTrg($table_old, $field_old, $table_new, $field_new) {
    try {
      $serial_value = $this->connection->query('SELECT PREVVAL FOR ' . $this->connection->makeSequenceName($old_table, $old_field) . ' FROM SYSIBM.SYSDUMMY1')->fetchField();
      $this->dropTrigger($table, $field, 'trg');
      $this->connection->query('DROP SEQUENCE ' . $this->connection->makeSequenceName($table, $field));
      $this->connection->query($this->_createSequenceSql($table, $field_new, $serial_value));
      $this->connection->query($this->_createTriggerSql($table, $field_new));
      
    }
    catch (Exception $e) {
      // No action required.
    }
    return TRUE;
  }

  protected function _createTriggerSql($table, $field) {
    $query = 'CREATE TRIGGER ' . $this->prefixNonTable($table, $field, 'trg') . ' NO CASCADE';
    $query .= ' BEFORE INSERT ON {' . $table . '}  REFERENCING NEW AS n';
    $query .= ' FOR EACH ROW ';
    $query .= ' SET n.' . $field . ' = NEXTVAL FOR ' . $this->connection->makeSequenceName($table, $field) . '';
    return $query;
  }

  protected function _createKeys($table, $new_keys) {
    if (isset($new_keys['primary key'])) {
      $this->addPrimaryKey($table, $new_keys['primary key']);
    }
    if (isset($new_keys['unique keys'])) {
      foreach ($new_keys['unique keys'] as $name => $fields) {
        $this->addUniqueKey($table, $name, $fields);
      }
    }
    if (isset($new_keys['indexes'])) {
      foreach ($new_keys['indexes'] as $name => $fields) {
        $this->addIndex($table, $name, $fields);
      }
    }
  }

  /**
   * Retrieve a table or column comment.
   */
  public function getComment($table, $column = NULL) {
    $table_name = $this->connection->prefixTables('{' . $table . '}');
    $table_name = $this->connection->escapeForDB2($table_name);
    $column_name = isset($column) ? $this->connection->escapeForDB2($column) : NULL;
		$db_user = $this->connection->getConnectionOptions()['username'];

    // Don't use {} around user_col_comments, user_tab_comments tables.
    if (isset($column_name)) {
      return $this->connection->query('SELECT COL.REMARKS FROM SYSCAT.COLUMNS COL LEFT JOIN SYSCAT.TABLES TAB ON COL.TABNAME = TAB.TABNAME AND COL.TABSCHEMA = TAB.TABSCHEMA WHERE LCASE(COL.TABNAME) = :table_name AND LCASE(COL.COLNAME) = :column_name AND LCASE(TAB.OWNER) = :db_user ', array(':table_name' => strtolower($table_name), ':column_name' => strtolower($column_name), ':db_user' => strtolower($db_user)))->fetchField();
    }
    else {
      return $this->connection->query('SELECT REMARKS FROM SYSCAT.TABLES WHERE LCASE(TABNAME) = :table_name AND LCASE(OWNER) = :db_user', array(':table_name' => strtolower($table_name), ':db_user' => strtolower($db_user)))->fetchField();
    }
  }
}

/**
 * @} End of "ingroup schemaapi".
 */
