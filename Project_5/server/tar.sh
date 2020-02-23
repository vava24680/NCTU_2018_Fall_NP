#!/bin/sh
mkdir application_server
tar -cvf application_server/application_server.tar --transform 's,^,application_server/,' amqp_client_thread/amqp_client_thread.py amqp_client_thread/__init__.py application_server.py base_server/base_server.py base_server/__init__.py application_server_requirement.txt config/server_config.yml
