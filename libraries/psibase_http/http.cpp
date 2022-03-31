// Adapted from Boost Beast Advanced Server example
//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "psibase/http.hpp"
#include "psibase/contract_entry.hpp"
#include "psibase/transaction_context.hpp"

#include <boost/asio/bind_executor.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/make_unique.hpp>
#include <boost/optional.hpp>

#include <eosio/finally.hpp>
#include <eosio/to_json.hpp>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace beast = boost::beast;                     // from <boost/beast.hpp>
namespace bhttp = beast::http;                      // from <boost/beast/http.hpp>
namespace net   = boost::asio;                      // from <boost/asio.hpp>
using tcp       = boost::asio::ip::tcp;             // from <boost/asio/ip/tcp.hpp>
using unixs = boost::asio::local::stream_protocol;  // from <boost/asio/local/stream_protocol.hpp>

using namespace std::literals;
using std::chrono::steady_clock;  // To create explicit timer

struct error_info
{
   int64_t          code    = {};
   std::string      name    = {};
   std::string      what    = {};
   std::vector<int> details = {};
};

EOSIO_REFLECT(error_info, code, name, what, details)

struct error_results
{
   uint16_t    code    = {};
   std::string message = {};
   error_info  error   = {};
};

EOSIO_REFLECT(error_results, code, message, error)

namespace psibase::http
{
   // Report a failure
   static void fail(beast::error_code ec, const char* what)
   {
      // TODO: elog("${w}: ${s}", ("w", what)("s", ec.message()));
   }

   // Return a reasonable mime type based on the extension of a file.
   beast::string_view mime_type(beast::string_view path)
   {
      using beast::iequals;
      const auto ext = [&path]
      {
         const auto pos = path.rfind(".");
         if (pos == beast::string_view::npos)
            return beast::string_view{};
         return path.substr(pos);
      }();
      if (iequals(ext, ".htm"))
         return "text/html";
      if (iequals(ext, ".html"))
         return "text/html";
      if (iequals(ext, ".php"))
         return "text/html";
      if (iequals(ext, ".css"))
         return "text/css";
      if (iequals(ext, ".txt"))
         return "text/plain";
      if (iequals(ext, ".js"))
         return "application/javascript";
      if (iequals(ext, ".json"))
         return "application/json";
      if (iequals(ext, ".wasm"))
         return "application/wasm";
      if (iequals(ext, ".xml"))
         return "application/xml";
      if (iequals(ext, ".swf"))
         return "application/x-shockwave-flash";
      if (iequals(ext, ".flv"))
         return "video/x-flv";
      if (iequals(ext, ".png"))
         return "image/png";
      if (iequals(ext, ".jpe"))
         return "image/jpeg";
      if (iequals(ext, ".jpeg"))
         return "image/jpeg";
      if (iequals(ext, ".jpg"))
         return "image/jpeg";
      if (iequals(ext, ".gif"))
         return "image/gif";
      if (iequals(ext, ".bmp"))
         return "image/bmp";
      if (iequals(ext, ".ico"))
         return "image/vnd.microsoft.icon";
      if (iequals(ext, ".tiff"))
         return "image/tiff";
      if (iequals(ext, ".tif"))
         return "image/tiff";
      if (iequals(ext, ".svg"))
         return "image/svg+xml";
      if (iequals(ext, ".svgz"))
         return "image/svg+xml";
      return "application/text";
   }  // mime_type

   // Append an HTTP rel-path to a local filesystem path.
   // The returned path is normalized for the platform.
   std::string path_cat(beast::string_view base, beast::string_view path)
   {
      if (base.empty())
         return std::string(path);
      std::string result(base);
#ifdef BOOST_MSVC
      char constexpr path_separator = '\\';
      if (result.back() == path_separator)
         result.resize(result.size() - 1);
      result.append(path.data(), path.size());
      for (auto& c : result)
         if (c == '/')
            c = path_separator;
#else
      char constexpr path_separator = '/';
      if (result.back() == path_separator)
         result.resize(result.size() - 1);
      result.append(path.data(), path.size());
#endif
      return result;
   }

   // This function produces an HTTP response for the given
   // request. The type of the response object depends on the
   // contents of the request, so the interface requires the
   // caller to pass a generic lambda for receiving the response.
   template <class Body, class Allocator, class Send>
   void handle_request(const http::http_config&                               http_config,
                       psibase::shared_state&                                 shared_state,
                       bhttp::request<Body, bhttp::basic_fields<Allocator>>&& req,
                       Send&&                                                 send)
   {
      // Returns a bad request response
      const auto bad_request = [&http_config, &req](beast::string_view why)
      {
         bhttp::response<bhttp::string_body> res{bhttp::status::bad_request, req.version()};
         res.set(bhttp::field::server, BOOST_BEAST_VERSION_STRING);
         res.set(bhttp::field::content_type, "text/html");
         if (!http_config.allow_origin.empty())
            res.set(bhttp::field::access_control_allow_origin, http_config.allow_origin);
         res.keep_alive(req.keep_alive());
         res.body() = why.to_string();
         res.prepare_payload();
         return res;
      };

      // Returns a not found response
      const auto not_found = [&http_config, &req](beast::string_view target)
      {
         bhttp::response<bhttp::string_body> res{bhttp::status::not_found, req.version()};
         res.set(bhttp::field::server, BOOST_BEAST_VERSION_STRING);
         res.set(bhttp::field::content_type, "text/html");
         if (!http_config.allow_origin.empty())
            res.set(bhttp::field::access_control_allow_origin, http_config.allow_origin);
         res.keep_alive(req.keep_alive());
         res.body() = "The resource '" + target.to_string() + "' was not found.";
         res.prepare_payload();
         return res;
      };

      // Returns an error response
      const auto error = [&http_config, &req](bhttp::status status, beast::string_view why,
                                              const char* content_type = "text/html")
      {
         bhttp::response<bhttp::string_body> res{status, req.version()};
         res.set(bhttp::field::server, BOOST_BEAST_VERSION_STRING);
         res.set(bhttp::field::content_type, content_type);
         if (!http_config.allow_origin.empty())
            res.set(bhttp::field::access_control_allow_origin, http_config.allow_origin);
         res.keep_alive(req.keep_alive());
         res.body() = why.to_string();
         res.prepare_payload();
         return res;
      };

      const auto ok = [&http_config, &req](std::vector<char> reply, const char* content_type)
      {
         bhttp::response<bhttp::vector_body<char>> res{bhttp::status::ok, req.version()};
         res.set(bhttp::field::server, BOOST_BEAST_VERSION_STRING);
         res.set(bhttp::field::content_type, content_type);
         if (!http_config.allow_origin.empty())
            res.set(bhttp::field::access_control_allow_origin, http_config.allow_origin);
         res.keep_alive(req.keep_alive());
         res.body() = std::move(reply);
         res.prepare_payload();
         return res;
      };

      try
      {
         auto host  = req.at("Host");
         auto colon = host.find(':');
         if (colon != host.npos)
            host.remove_suffix(host.size() - colon);

         if (req.target() == "/" || req.target().starts_with("/rpc") ||
             req.target().starts_with("/roothost") || req.target().starts_with("/ui") ||
             host != http_config.host && host.ends_with(http_config.host))
         {
            rpc_request_data data;
            if (req.method() == bhttp::verb::get)
               data.method = "GET";
            else if (req.method() == bhttp::verb::post)
               data.method = "POST";
            else
               return send(error(bhttp::status::bad_request,
                                 "Unsupported HTTP-method for " + req.target().to_string() + "\n"));
            data.host      = {host.begin(), host.size()};
            data.root_host = http_config.host;
            data.target    = req.target().to_string();
            data.body      = req.body();

            // TODO: time limit
            auto           system = shared_state.get_system_context();
            eosio::finally f{[&]() { shared_state.add_system_context(std::move(system)); }};
            block_context  bc{*system, read_only{}};
            bc.start();
            signed_transaction trx;
            action             act{
                            .sender   = AccountNumber(),
                            .contract = rpcContractNum,
                            .raw_data = psio::convert_to_frac(data),
            };
            transaction_trace   trace;
            transaction_context tc{bc, trx, {}, trace, false};
            action_trace        atrace;
            tc.exec_rpc(act, atrace);
            auto result = psio::convert_from_frac<rpc_reply_data>(atrace.raw_retval);
            return send(ok(std::move(result.reply), result.contentType.c_str()));
         }
         else if (http_config.static_dir.empty())
         {
            return send(error(bhttp::status::not_found,
                              "The resource '" + req.target().to_string() + "' was not found.\n"));
         }
         else
         {
            // Make sure we can handle the method
            if (req.method() != bhttp::verb::get && req.method() != bhttp::verb::head)
               return send(bad_request("Unknown HTTP-method"));

            // Request path must be absolute and not contain "..".
            if (req.target().empty() || req.target()[0] != '/' ||
                req.target().find("..") != beast::string_view::npos)
               return send(bad_request("Illegal request-target"));

            // Build the path to the requested file
            std::string path = path_cat(http_config.static_dir, req.target());
            if (req.target().back() == '/')
               path.append("index.html");

            // Attempt to open the file
            beast::error_code            ec;
            bhttp::file_body::value_type body;
            body.open(path.c_str(), beast::file_mode::scan, ec);

            // Handle the case where the file doesn't exist
            if (ec == beast::errc::no_such_file_or_directory)
               return send(not_found(req.target()));

            // Handle an unknown error
            if (ec)
               return send(error(bhttp::status::internal_server_error,
                                 "An error occurred: "s + ec.message()));

            // Cache the size since we need it after the move
            const auto size = body.size();

            // Respond to HEAD request
            if (req.method() == bhttp::verb::head)
            {
               bhttp::response<bhttp::empty_body> res{bhttp::status::ok, req.version()};
               res.set(bhttp::field::server, BOOST_BEAST_VERSION_STRING);
               res.set(bhttp::field::content_type, mime_type(path));
               if (!http_config.allow_origin.empty())
                  res.set(bhttp::field::access_control_allow_origin, http_config.allow_origin);
               res.content_length(size);
               res.keep_alive(req.keep_alive());
               return send(std::move(res));
            }

            // Respond to GET request
            bhttp::response<bhttp::file_body> res{
                std::piecewise_construct, std::make_tuple(std::move(body)),
                std::make_tuple(bhttp::status::ok, req.version())};
            res.set(bhttp::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(bhttp::field::content_type, mime_type(path));
            if (!http_config.allow_origin.empty())
               res.set(bhttp::field::access_control_allow_origin, http_config.allow_origin);
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
         }
      }
      catch (const std::exception& e)
      {
         try
         {
            // elog("query failed: ${s}", ("s", e.what()));
            error_results err;
            err.code       = (uint16_t)bhttp::status::internal_server_error;
            err.message    = "Internal Service Error";
            err.error.name = "exception";
            err.error.what = e.what();
            return send(error(bhttp::status::internal_server_error, eosio::convert_to_json(err),
                              "application/json"));
         }
         catch (...)
         {  //
            return send(error(bhttp::status::internal_server_error,
                              "failure reporting exception failure\n"));
         }
      }
      catch (...)
      {
         // TODO: elog("query failed: unknown exception");
         return send(
             error(bhttp::status::internal_server_error, "query failed: unknown exception\n"));
      }
   }  // handle_request

   // Handles an HTTP server connection
   template <typename SessionType>
   class http_session
   {
      // This queue is used for HTTP pipelining.
      class queue
      {
         enum
         {
            // Maximum number of responses we will queue
            limit = 8
         };

         // The type-erased, saved work item
         struct work
         {
            virtual ~work()           = default;
            virtual void operator()() = 0;
         };

         http_session&                      self;
         std::vector<std::unique_ptr<work>> items;

        public:
         explicit queue(http_session& self) : self(self)
         {
            static_assert(limit > 0, "queue limit must be positive");
            items.reserve(limit);
         }

         // Returns `true` if we have reached the queue limit
         bool is_full() const { return items.size() >= limit; }

         // Called when a message finishes sending
         // Returns `true` if the caller should initiate a read
         bool on_write()
         {
            BOOST_ASSERT(!items.empty());
            const auto was_full = is_full();
            items.erase(items.begin());
            if (!items.empty())
               (*items.front())();
            return was_full;
         }

         // Called by the HTTP handler to send a response.
         template <bool isRequest, class Body, class Fields>
         void operator()(bhttp::message<isRequest, Body, Fields>&& msg)
         {
            // This holds a work item
            struct work_impl : work
            {
               http_session&                           self;
               bhttp::message<isRequest, Body, Fields> msg;

               work_impl(http_session& self, bhttp::message<isRequest, Body, Fields>&& msg)
                   : self(self), msg(std::move(msg))
               {
               }

               void operator()()
               {
                  bhttp::async_write(
                      self.derived_session().stream, msg,
                      beast::bind_front_handler(&http_session::on_write,
                                                self.derived_session().shared_from_this(),
                                                msg.need_eof()));
               }
            };

            // Allocate and store the work
            items.push_back(boost::make_unique<work_impl>(self, std::move(msg)));

            // If there was no previous work, start this one
            if (items.size() == 1)
               (*items.front())();
         }
      };

      beast::flat_buffer                       buffer;
      std::shared_ptr<const http::http_config> http_config;
      std::shared_ptr<psibase::shared_state>   shared_state;
      queue                                    queue_;
      std::unique_ptr<net::steady_timer>       _timer;
      steady_clock::time_point                 last_activity_timepoint;

      // The parser is stored in an optional container so we can
      // construct it from scratch it at the beginning of each new message.
      boost::optional<bhttp::request_parser<bhttp::vector_body<char>>> parser;

     public:
      // Take ownership of the socket
      http_session(const std::shared_ptr<const http::http_config>& http_config,
                   const std::shared_ptr<psibase::shared_state>&   shared_state)
          : http_config(http_config), shared_state(shared_state), queue_(*this)
      {
      }

      // Start the session
      void run()
      {
         _timer.reset(
             new boost::asio::steady_timer(derived_session().stream.socket().get_executor()));
         last_activity_timepoint = steady_clock::now();
         start_socket_timer();
         do_read();
      }

     private:
      SessionType& derived_session() { return static_cast<SessionType&>(*this); }

      void start_socket_timer()
      {
         _timer->expires_after(http_config->idle_timeout_ms);
         _timer->async_wait(
             [this](beast::error_code ec)
             {
                if (ec)
                {
                   return;
                }
                auto session_duration = steady_clock::now() - last_activity_timepoint;
                if (session_duration <= http_config->idle_timeout_ms)
                {
                   start_socket_timer();
                }
                else
                {
                   ec = beast::error::timeout;
                   fail(ec, "timeout");
                   return do_close();
                }
             });
      }

      void do_read()
      {
         // Construct a new parser for each message
         parser.emplace();

         // Apply a reasonable limit to the allowed size
         // of the body in bytes to prevent abuse.
         // todo: make configurable
         parser->body_limit(http_config->max_request_size);
         last_activity_timepoint = steady_clock::now();
         // Read a request using the parser-oriented interface
         bhttp::async_read(derived_session().stream, buffer, *parser,
                           beast::bind_front_handler(&http_session::on_read,
                                                     derived_session().shared_from_this()));
      }

      void on_read(beast::error_code ec, std::size_t bytes_transferred)
      {
         boost::ignore_unused(bytes_transferred);

         // This means they closed the connection
         if (ec == bhttp::error::end_of_stream)
            return do_close();

         if (ec)
         {
            fail(ec, "read");
            return do_close();
         }

         // Send the response
         handle_request(*http_config, *shared_state, parser->release(), queue_);

         // If we aren't at the queue limit, try to pipeline another request
         if (!queue_.is_full())
            do_read();
      }

      void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
      {
         boost::ignore_unused(bytes_transferred);

         if (ec)
         {
            fail(ec, "write");
            do_close();
         }

         if (close)
         {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
         }

         // Inform the queue that a write completed
         if (queue_.on_write())
         {
            // Read another request
            do_read();
         }
      }

      void do_close()
      {
         // Send a TCP shutdown
         beast::error_code ec;
         derived_session().stream.socket().shutdown(tcp::socket::shutdown_send, ec);
         _timer->cancel();  // cancel connection timer.
         // At this point the connection is closed gracefully
      }
   };  // http_session

   struct tcp_http_session : public http_session<tcp_http_session>,
                             public std::enable_shared_from_this<tcp_http_session>
   {
      tcp_http_session(const std::shared_ptr<const http::http_config>& http_config,
                       const std::shared_ptr<psibase::shared_state>&   shared_state,
                       tcp::socket&&                                   socket)
          : http_session<tcp_http_session>(http_config, shared_state), stream(std::move(socket))
      {
      }

      beast::tcp_stream stream;
   };

   struct unix_http_session : public http_session<unix_http_session>,
                              public std::enable_shared_from_this<unix_http_session>
   {
      unix_http_session(const std::shared_ptr<const http::http_config>& http_config,
                        const std::shared_ptr<psibase::shared_state>&   shared_state,
                        unixs::socket&&                                 socket)
          : http_session<unix_http_session>(http_config, shared_state), stream(std::move(socket))
      {
      }

      beast::basic_stream<unixs,
#if BOOST_VERSION >= 107400
                          boost::asio::any_io_executor,
#else
                          boost::asio::executor,
#endif
                          beast::unlimited_rate_policy>
          stream;
   };

   // Accepts incoming connections and launches the sessions
   class listener : public std::enable_shared_from_this<listener>
   {
      std::shared_ptr<const http::http_config> http_config;
      std::shared_ptr<psibase::shared_state>   shared_state;
      net::io_context&                         ioc;
      tcp::acceptor                            tcp_acceptor;
      unixs::acceptor                          unix_acceptor;
      bool                                     acceptor_ready = false;

     public:
      listener(const std::shared_ptr<const http::http_config>& http_config,
               const std::shared_ptr<psibase::shared_state>&   shared_state,
               net::io_context&                                ioc)
          : http_config{http_config},
            shared_state{shared_state},
            ioc(ioc),
            tcp_acceptor(net::make_strand(ioc)),
            unix_acceptor(net::make_strand(ioc))
      {
         if (http_config->address.size())
         {
            boost::asio::ip::address a;
            try
            {
               a = net::ip::make_address(http_config->address);
            }
            catch (std::exception& e)
            {
               throw std::runtime_error("make_address(): "s + http_config->address + ": " +
                                        e.what());
            }

            start_listen(tcp_acceptor, tcp::endpoint{a, http_config->port});
         }

         if (http_config->unix_path.size())
         {
            //take a sniff and see if anything is already listening at the given socket path, or if the socket path exists
            // but nothing is listening
            boost::system::error_code test_ec;
            unixs::socket             test_socket(ioc);
            test_socket.connect(http_config->unix_path.c_str(), test_ec);

            //looks like a service is already running on that socket, don't touch it... fail out
            if (test_ec == boost::system::errc::success)
               eosio::check(false, "wasmql http unix socket is in use");
            //socket exists but no one home, go ahead and remove it and continue on
            else if (test_ec == boost::system::errc::connection_refused)
               ::unlink(http_config->unix_path.c_str());
            else if (test_ec != boost::system::errc::no_such_file_or_directory)
               eosio::check(false,
                            "unexpected failure when probing existing wasmql http unix socket: " +
                                test_ec.message());

            start_listen(unix_acceptor, unixs::endpoint(http_config->unix_path));
         }

         acceptor_ready = true;
      }

      template <typename Acceptor, typename Endpoint>
      void start_listen(Acceptor& acceptor, const Endpoint& endpoint)
      {
         beast::error_code ec;

         auto check_ec = [&](const char* what)
         {
            if (!ec)
               return;
            // TODO: elog("${w}: ${m}", ("w", what)("m", ec.message()));
            eosio::check(false, "unable to open listen socket");
         };

         // Open the acceptor
         acceptor.open(endpoint.protocol(), ec);
         check_ec("open");

         // Bind to the server address
         acceptor.set_option(net::socket_base::reuse_address(true));
         acceptor.bind(endpoint, ec);
         check_ec("bind");

         // Start listening for connections
         acceptor.listen(net::socket_base::max_listen_connections, ec);
         check_ec("listen");
      }

      // Start accepting incoming connections
      bool run()
      {
         if (!acceptor_ready)
            return acceptor_ready;
         if (tcp_acceptor.is_open())
            do_accept(tcp_acceptor);
         if (unix_acceptor.is_open())
            do_accept(unix_acceptor);
         return acceptor_ready;
      }

     private:
      template <typename Acceptor>
      void do_accept(Acceptor& acceptor)
      {
         // The new connection gets its own strand
         acceptor.async_accept(
             net::make_strand(ioc),
             beast::bind_front_handler(
                 [&acceptor, self = shared_from_this(), this](beast::error_code ec,
                                                              auto              socket) mutable
                 {
                    if (ec)
                    {
                       fail(ec, "accept");
                    }
                    else
                    {
                       // Create the http session and run it
                       if constexpr (std::is_same_v<Acceptor, tcp::acceptor>)
                       {
                          boost::system::error_code ec;
                          // TODO:
                          // dlog("Accepting connection from ${ra}:${rp} to ${la}:${lp}",
                          //      ("ra", socket.remote_endpoint(ec).address().to_string())(
                          //          "rp", socket.remote_endpoint(ec).port())(
                          //          "la", socket.local_endpoint(ec).address().to_string())(
                          //          "lp", socket.local_endpoint(ec).port()));
                          std::make_shared<tcp_http_session>(http_config, shared_state,
                                                             std::move(socket))
                              ->run();
                       }
                       else if constexpr (std::is_same_v<Acceptor, unixs::acceptor>)
                       {
                          boost::system::error_code ec;
                          auto                      rep = socket.remote_endpoint(ec);
                          // TODO: dlog("Accepting connection from ${r}", ("r", rep.path()));
                          std::make_shared<unix_http_session>(http_config, shared_state,
                                                              std::move(socket))
                              ->run();
                       }
                    }

                    // Accept another connection
                    do_accept(acceptor);
                 }));
      }
   };  // listener

   struct server_impl : server, std::enable_shared_from_this<server_impl>
   {
      net::io_service                          ioc;
      std::shared_ptr<const http::http_config> http_config  = {};
      std::shared_ptr<psibase::shared_state>   shared_state = {};
      std::vector<std::thread>                 threads      = {};

      server_impl(const std::shared_ptr<const http::http_config>& http_config,
                  const std::shared_ptr<psibase::shared_state>&   shared_state)
          : http_config{http_config}, shared_state{shared_state}
      {
      }

      virtual ~server_impl() {}

      virtual void stop() override
      {
         ioc.stop();
         for (auto& t : threads)
            t.join();
         threads.clear();
      }

      bool start()
      {
         auto l = std::make_shared<listener>(http_config, shared_state, ioc);
         if (!l->run())
            return false;

         threads.reserve(http_config->num_threads);
         for (unsigned i = 0; i < http_config->num_threads; ++i)
            threads.emplace_back([self = shared_from_this()] { self->ioc.run(); });
         return true;
      }
   };  // server_impl

   std::shared_ptr<server> server::create(const std::shared_ptr<const http_config>& http_config,
                                          const std::shared_ptr<shared_state>&      shared_state)
   {
      eosio::check(http_config->num_threads > 0, "too few threads");
      auto server = std::make_shared<server_impl>(http_config, shared_state);
      if (server->start())
         return server;
      else
         return nullptr;
   }

}  // namespace psibase::http
