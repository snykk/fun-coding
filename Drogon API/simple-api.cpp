#include <drogon/drogon.h>
#include <drogon/HttpAppFramework.h>
#include <drogon/drogon_jwt.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <pqxx/pqxx>

using json = nlohmann::json;

int main() {
    // Initialize the Drogon application
    drogon::app().setLogPath("logs");
    drogon::app().setLogLevel(trantor::Logger::WARN);

    // Set the secret key used for JWT authentication
    drogon::app().setJwtSecret("my_secret_key");

    // Set up a database connection to PostgreSQL
    pqxx::connection db_connection("postgresql://user:password@localhost/mydb");

    // Set up a JWT authentication middleware for the API
    drogon::app().registerMiddleware(drogon::MiddlewareType::PreRouting, [](const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& cb, const drogon::WebSocketConnectionPtr&) {
        try {
            // Exclude the login and registration endpoints from authentication
            auto path = req->path();
            if (path == "/register" || path == "/login") {
                cb();
                return;
            }

            // Extract the JWT token from the Authorization header
            auto auth_header = req->getHeader("Authorization");
            if (auth_header.empty()) {
                throw drogon::Exception(drogon::StatusCode::kUnauthorized, "Missing authorization header");
            }
            auto auth_parts = drogon::utils::splitString(auth_header, " ");
            if (auth_parts.size() != 2 || auth_parts[0] != "Bearer") {
                throw drogon::Exception(drogon::StatusCode::kUnauthorized, "Invalid authorization header format");
            }
            auto token = auth_parts[1];

            // Verify the JWT token
            auto payload = drogon::jwt::verifyJwtToken(token, drogon::app().getJwtSecret());

            // Store the user ID from the JWT token in the request context for later use
            req->setContext("user_id", payload.getClaim<int>("user_id"));

            // Call the next middleware or handler
            cb();
        }
        catch (const drogon::Exception& e) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(e.status());
            resp->setBody(e.what());
            cb(resp);
        }
        catch (...) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::StatusCode::kInternalServerError);
            resp->setBody("Internal server error");
            cb(resp);
        }
    });

        // Define a registration endpoint at the path "/register" that creates a new user account
    drogon::app().registerHandler("/register", [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Parse the JSON request body
        json body;
        try {
            body = json::parse(req->getBody());
        }
        catch (const json::exception& e) {
            throw drogon::Exception(drogon::StatusCode::kBadRequest, "Invalid request body");
        }

        // Extract the registration data from the request body
        auto name = body.value("name", "");
        auto email = body.value("email", "");
        auto password = body.value("password", "");

        // Validate the registration data
        if (name.empty() || email.empty() || password.empty()) {
            throw drogon::Exception(drogon::StatusCode::kBadRequest, "Missing registration data");
        }

        try {
            // Insert the new user account into the database
            pqxx::work w(db_connection);
            auto result = w.exec_prepared("insert_user", name, email, drogon::utils::getMd5(password));
            w.commit();
        }
        catch (const pqxx::unique_violation& e) {
            throw drogon::Exception(drogon::StatusCode::kConflict, "Email address already in use");
        }
        catch (const pqxx::sql_error& e) {
            throw drogon::Exception(drogon::StatusCode::kInternalServerError, "Database error");
        }

        // Send a response indicating successful registration
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::StatusCode::kCreated);
        callback(resp);
    });

    // Define a login endpoint at the path "/login" that authenticates a user and returns a JWT token
    drogon::app().registerHandler("/login", [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Parse the JSON request body
        json body;
        try {
            body = json::parse(req->getBody());
        }
        catch (const json::exception& e) {
            throw drogon::Exception(drogon::StatusCode::kBadRequest, "Invalid request body");
        }

        // Extract the login credentials from the request body
        auto email = body.value("email", "");
        auto password = body.value("password", "");

        // Find the user with the specified email address
        auto db = drogon::app().getDbClient();
        db->execSqlAsync("SELECT * FROM users WHERE email = $1", [callback, email, password](const drogon::orm::Result& result) {
            if (result.size() != 1) {
                throw drogon::Exception(drogon::StatusCode::kUnauthorized, "Invalid email or password");
            }

            // Verify the password hash
            auto user = result[0];
            if (!bcrypt::validate_password(password, user["password"].as<std::string>())) {
                throw drogon::Exception(drogon::StatusCode::kUnauthorized, "Invalid email or password");
            }

            // Generate a JWT token for the user
            drogon::jwt::Token payload;
            payload.addClaim("user_id", user["id"].as<std::string>());
            auto token = drogon::jwt::createJwtToken(payload, drogon::app().getJwtSecret());

            // Send a response containing the JWT token
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::StatusCode::kOK);
            resp->addHeader("Content-Type", "application/json");
            resp->setBody({{"token", token}}.dump());
            callback(resp);
        }, email);
    });

     // Define a handler for the authenticated endpoint
    drogon::app().registerHandler("/authenticatedEndpoint", [](const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        // Get the user ID from the request context
        auto user_id = req->getContext<int>("user_id");

        // Get data for the authenticated user from the database
        pqxx::work txn(db_connection);
        pqxx::result result = txn.exec("SELECT * FROM users WHERE id = " + txn.quote(user_id));
        if (result.empty()) {
            throw drogon::Exception(drogon::StatusCode::kNotFound, "User not found");
        }
        auto user_data = result[0];
        txn.commit();

        // Build a JSON response containing the user's data
        json response_data = {
            {"name", user_data["name"].as<std::string>()},
            {"email", user_data["email"].as<std::string>()}
        };

        // Build an
        // Build an HTTP response containing the JSON data
        auto resp = drogon::HttpResponse::newHttpJsonResponse(response_data);
        callback(resp);
    });
}