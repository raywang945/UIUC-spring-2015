openssl genrsa -out the_servers_key.key 1024
openssl req -new -key the_servers_key.key -out temp_request.csr
openssl x509 -req -days 365 -in temp_request.csr -signkey the_servers_key.key -out the_servers_cert.crt
