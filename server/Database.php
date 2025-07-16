<?php

class Database
{
    private $conn;

    /**
     * Establishes the database connection using a configuration array.
     * @param array $config The database configuration.
     * Expected keys: 'host', 'db_name', 'username', 'password'.
     */
    public function __construct(array $config) {
        $this->conn = null;
        try {
            // Data Source Name (DSN)
            $dsn = 'mysql:host=' . $config['host'] . ';dbname=' . $config['name'];
            
            // Create a PDO instance
            $this->conn = new PDO($dsn, $config['user'], $config['pass']);
            
            // Set the PDO error mode to exception
            $this->conn->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        } catch (PDOException $e) {
            // It's generally better to log errors than to echo them directly.
            error_log('Connection Error: ' . $e->getMessage());
            // You might want to throw the exception or handle it more gracefully.
            // For simplicity, we'll just echo it here for immediate feedback.
            echo 'Connection Error: ' . $e->getMessage();
        }
    }

    /**
     * Executes a SELECT query and fetches all results.
     *
     * @param string $query The SQL SELECT statement.
     * @param array $params The parameters to bind to the query.
     * @return array The result set as an associative array.
     */
    public function select(string $query, array $params = []): array
    {
        try {
            $stmt = $this->conn->prepare($query);
            $stmt->execute($params);
            return $stmt->fetchAll(PDO::FETCH_ASSOC);
        } catch (PDOException $e) {
            error_log('Query Error: ' . $e->getMessage());
            return []; // Return an empty array on failure
        }
    }

    /**
     * Executes an INSERT query.
     *
     * @param string $query The SQL INSERT statement.
     * @param array $params The parameters to bind to the query.
     * @return bool True on success, false on failure.
     */
    public function insert(string $query, array $params = []): bool
    {
        try {
            $stmt = $this->conn->prepare($query);
            return $stmt->execute($params);
        } catch (PDOException $e) {
            error_log('Query Error: ' . $e->getMessage());
            return false;
        }
    }

    /**
     * Executes an UPDATE query.
     *
     * @param string $query The SQL UPDATE statement.
     * @param array $params The parameters to bind to the query.
     * @return bool True on success, false on failure.
     */
    public function update(string $query, array $params = []): bool
    {
        try {
            $stmt = $this->conn->prepare($query);
            return $stmt->execute($params);
        } catch (PDOException $e) {
            error_log('Query Error: ' . $e->getMessage());
            return false;
        }
    }

    /**
     * Closes the database connection.
     */
    public function __destruct()
    {
        $this->conn = null;
    }
}

/*
// --- Example Usage ---

// 1. Define your database configuration
$dbConfig = [
    'host' => 'localhost',
    'db_name' => 'test_db',
    'username' => 'root',
    'password' => 'password'
];

// 2. Create a new Database instance
$db = new Database($dbConfig);

// 3. Example: Select all users
$users = $db->select('SELECT * FROM users');
print_r($users);

// 4. Example: Insert a new user
$success = $db->insert(
    'INSERT INTO users (name, email) VALUES (?, ?)',
    ['John Doe', 'john.doe@example.com']
);
if ($success) {
    echo "User inserted successfully.";
}

// 5. Example: Update a user's email
$updated = $db->update(
    'UPDATE users SET email = ? WHERE name = ?',
    ['new.email@example.com', 'John Doe']
);
if ($updated) {
    echo "User updated successfully.";
}

*/
