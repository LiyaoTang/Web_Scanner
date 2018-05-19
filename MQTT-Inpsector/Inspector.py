# coding: utf-8
import paho.mqtt.client as mqtt
from optparse import OptionParser
import time
import sys
import gc


parser = OptionParser()
parser.add_option("--qos", type="int", dest="qos")
parser.add_option("--speed", dest="speed")

parser.add_option("--port", type="int", default=1883, dest="port")
parser.add_option("--bind", default="", dest="bind")
(options, args) = parser.parse_args()

qos = options.qos
speed = options.speed

port = options.port
bind = options.bind
assert qos in [0,1,2]
assert speed in ['fast', 'slow', 'SYS']

client_id = '3310-u6142160' +'_' + speed + str(qos) # unique ID required
client_name = '3310student'
client_password = 'comp3310'
server_addr = '3310exp.hopto.org'

init_tmp_record   = lambda: {'cnt':0, 'valid_cnt':0, 'loss_cnt':0, 'dup_cnt':0, 'mis_order_cnt':0, 
                             'cur_val':-1, 'newest_T':-1, 'start_time':time.time(), 'max_T':60}

init_topic_record = lambda topic, qos :{'1min' : init_tmp_record(),
                                        '10min': {'recv':0, 'loss':0, 'dupe':0, 'ooo':0, 
                                                  'start_time':time.time(), 'max_T':60*10},
                                        'topic': topic, 
                                        'qos'  : str(qos)}

def publish_result(client, record_long, record_qos, cur_time, qos=2, rt_f=True, root_topic='studentreport/u6142160/'):
    # t 10-minute intervals under
    # studentreport/<your.Uni.ID>/ with the ‘retain’ flag set and QoS=2.
    
    client.publish(root_topic + 'language', payload='python', qos=qos, retain=rt_f)
    client.publish(root_topic + 'timestamp', payload=cur_time, qos=qos, retain=rt_f)
    for key in ['recv', 'loss', 'dupe', 'ooo']:
        client.publish(root_topic + str(cur_time) + '/' + record_qos + '/' + key, payload=record_long[key], qos=qos, retain=rt_f)

    print('info-10min:')
    for key in record_long.keys():
        print(key, record_long[key])
    print('end_time', cur_time)
    print('-------')
    print()
    sys.stdout.flush()

def collect_1min_info(client, userdata, cur_time):

    record_tmp  = userdata['1min']
    record_long = userdata['10min']

    print('info-1min:')
    for key in record_tmp.keys():
        print(key, record_tmp[key])
    print()

    recv = record_tmp['cnt']
    loss = record_tmp['loss_cnt'] / record_tmp['cnt']
    dupe = record_tmp['dup_cnt']  / record_tmp['cnt']
    ooo  = record_tmp['mis_order_cnt'] / record_tmp['cnt']

    if recv > record_long['recv']: record_long['recv'] = recv
    if loss > record_long['loss']: record_long['loss'] = loss
    if dupe > record_long['dupe']: record_long['dupe'] = dupe
    if ooo  > record_long['ooo']:  record_long['ooo']  = ooo

    if cur_time - record_long['start_time'] >= record_long['max_T']:
        publish_result(client, record_long, userdata['qos'], cur_time)
        userdata = init_topic_record(userdata['topic'], userdata['qos'])
    else:
        userdata['1min'] = init_tmp_record()

    client.user_data_set(userdata)
    gc.collect()

# client's event on receiving message
def on_message_qos_2(client, userdata, msg):
    # qos=2 => no out-of-order & dup (loss due to limited buffer)
    if msg.topic != userdata['topic']: return # in case of sub-topic
    record = userdata['1min']

    try:
        cur_val = int(msg.payload)
    except:
        print("received ", msg.payload)
        return

    record['cnt'] += 1
    if msg.timestamp > record['newest_T']: # in time series
        record['newest_T'] = msg.timestamp
        
        if cur_val == record['cur_val']+1 or record['cur_val'] == -1 or cur_val == 0: # expected
            # +1 / 1st msg / wrap around 
            record['cur_val'] = cur_val
            record['valid_cnt'] += 1
            
        else:
            if cur_val > record['cur_val']: # loss (due to limited buffer)
                record['loss_cnt'] += cur_val - record['cur_val'] - 1
                record['cur_val'] = cur_val
            elif cur_val == record['cur_val']: # dup
                record['dup_cnt'] += 1
            else: # loss & wrap
                record['loss_cnt'] += cur_val - 1
                record['cur_val'] = cur_val
                
    else: # should come in the past => out of order => can't tell if not lost
        record['mis_order_cnt'] += 1
    
    # collect 1-min info
    cur_time = time.time()
    if cur_time - record['start_time'] >= record['max_T']:
        collect_1min_info(client, userdata, cur_time)

# client's event on receiving message
def on_message_qos_1(client, userdata, msg):
    # qos=1 => no out-of-order (loss due to limited buffer)
    if msg.topic != userdata['topic']: return # in case of sub-topic
    record = userdata['1min']
    
    try:
        cur_val = int(msg.payload)
    except:
        print("received ", msg.payload)
        return

    record['cnt'] += 1
    if msg.timestamp > record['newest_T']: # in time series
        record['newest_T'] = msg.timestamp
        
        if cur_val == record['cur_val']+1 or record['cur_val'] == -1 or cur_val == 0: # expected
            # +1 / 1st msg / wrap around 
            record['cur_val'] = cur_val
            record['valid_cnt'] += 1
            
        else:
            if cur_val > record['cur_val']: # loss (due to limited buffer)
                record['loss_cnt'] += cur_val - record['cur_val'] - 1
                record['cur_val'] = cur_val
            elif cur_val == record['cur_val']: # dup
                record['dup_cnt'] += 1
            else: # loss & wrap
                record['loss_cnt'] += cur_val - 1
                record['cur_val'] = cur_val
                
    else: # should come in the past => out of order => can't tell if not lost
        record['mis_order_cnt'] += 1
        
    # collect 1-min info
    cur_time = time.time()
    if cur_time - record['start_time'] >= record['max_T']:
        collect_1min_info(client, userdata, cur_time)
    
# client's event on receiving message
def on_message_qos_0(client, userdata, msg):
    # qos=0 => no dup
    if msg.topic != userdata['topic']: return # in case of sub-topic
    record = userdata['1min']
    
    try:
        cur_val = int(msg.payload)
    except:
        print("received ", msg.payload)
        return

    record['cnt'] += 1
    if msg.timestamp > record['newest_T']: # expected order => increasing number

        if cur_val == record['cur_val']+1 or record['cur_val'] == -1 or cur_val == 0: # expected
            # +1 / 1st msg / wrap around
            record['valid_cnt'] += 1
        elif cur_val > record['cur_val']: # lossing in-between
            record['loss_cnt'] += cur_val - record['cur_val'] - 1
        else: # lossing 0...
            record['loss_cnt'] += cur_val-1

        record['cur_val'] = cur_val
        record['newest_T'] = msg.timestamp
        
    else: # out-of-order => not lost
        record['mis_order_cnt'] += 1
        record['loss_cnt'] -= 1
    
    # collect 1-min info
    cur_time = time.time()
    if cur_time - record['start_time'] >= record['max_T']:
        collect_1min_info(client, userdata, cur_time)

def on_message_SYS(client, userdata, msg):
    print(time.time(), msg.topic, msg.payload)
    sys.stdout.flush()

def subscribe_counter(client, userdata, speed, qos, topic=None):
    if speed == 'SYS':
        topic = '$SYS/broker/#'
        assert qos == 2
    else:
        topic = 'counter/' + speed + '/q' + str(qos)

    sub_rst = None
    while(sub_rst != mqtt.MQTT_ERR_SUCCESS):
        sub_rst, _ = client.subscribe(topic, qos=qos)
            
    # init record after successfullt subscribe => deteministic time shift of actual successful subscribe
    userdata = init_topic_record(topic, qos)
    client.user_data_set(userdata)

    print('sucessfully subscribe ', topic)

# client's event on connect
def on_connect(client, userdata, flags, rc):
    '''
        flags['session present'] = 1 => last session retained by broker, otherwise a new session

        rc 0=Connection successful, 1=incorrect protocol version, 2=invalid client identifier,
           3=server unavailable,    4=bad username or password,   5=not authorised

        userdata: created along with Client class initiliasation
    '''

    # subscribe topic (speed & qos : global visible)
    subscribe_counter(client, userdata, speed, qos)

# client's event on disconnect
def on_disconnect(client, userdata, rc):
    client.loop_stop()

    print('disconnect time', time.time())
    
    # print log
    if rc != 0:
        print('Unexpected disconnection: rc =', rc)
    print('log:')
    
    topic = userdata['topic']
    if 'SYS' not in topic:
        for record in ['1min', '10min']:
            print("--------", record, "---------")
            record = userdata[record]
            for key in record.keys():
                print(key, record[key])


if speed == 'SYS':
    on_message = on_message_SYS
else:
    if qos == 0:
        on_message = on_message_qos_0
    elif qos == 1:
        on_message = on_message_qos_1
    elif qos == 2:
        on_message = on_message_qos_2


# initialisation
client = mqtt.Client(client_id, clean_session=True, transport="tcp")
client.username_pw_set(username=client_name, password=client_password)

client.on_connect    = on_connect
client.on_message    = on_message
client.on_disconnect = on_disconnect

# potentially blocking => connect_async for non-blocking
client.connect(host=server_addr, port=port, keepalive=60, bind_address=bind)
# keepalive:
#     maximum period in seconds allowed between communications with the broker. 
#     If no other messages are being exchanged, this controls the rate at which the client will send 
#     ping messages to the broker

gc.collect()

client.loop_forever()
