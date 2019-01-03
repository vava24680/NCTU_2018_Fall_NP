import pika


credential = pika.PlainCredentials('np_hw4_user', 'np_hw4')
parameter = pika.ConnectionParameters(host = 'npaws.haohao.in', virtual_host = 'np_hw4', credentials = credential)
connection = pika.BlockingConnection(parameter)
channel = connection.channel()
#properties = pika.spec.BasicProperties(type = 'public')
properties = pika.spec.BasicProperties(type = 'private')
channel.basic_publish(exchange='personal',
                      routing_key='haohaoA',
                      properties = properties,
                      body='Hello haohaoA')
"""
channel.basic_publish(exchange='china_pub',
                      routing_key='haohaoB',
                      properties = properties,
                      body='[PUBLIC] Hello World!!!')
"""
print(" [x] Sent Done!!")

connection.close()
