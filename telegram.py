# Source https://github.com/RaghuVarma331/scripts/blob/master/pythonscripts/telegram.py
from argparse import ArgumentParser
from requests import post


def arg_parse():
    global token, chat, file, mode, preview, caption, silent, send, out
    switches = ArgumentParser()
    switches.add_argument("-F", "--file", required=True, help="File path")
    switches.add_argument("-t", "--token", required=True, help="Telegram bot token")
    switches.add_argument("-c", "--chat", required=True, help="Chat to use as recipient")
    switches.add_argument("-m", "--mode", help="Text parse mode - HTML/Markdown", default="Markdown")
    switches.add_argument("-p", "--preview", help="Disable URL preview - yes/no", default="yes")
    switches.add_argument("-s", "--silent", help="Disable Notification Sound - yes/no", default="no")
    switches.add_argument("-d", "--output", help="Disable Script output - yes/no", default="yes")
    switches.add_argument("-C", "--caption", help="Media/Document caption")

    args = vars(switches.parse_args())
    token = args["token"]
    chat = args["chat"]
    file = args["file"]
    mode = args["mode"]
    preview = args["preview"]
    silent = args["silent"]
    out = args["output"]
    caption = args["caption"]

    if file is not None:
        send = "file"

def send_message():
    global r, status, response
    if send == "file":
        files = {
            'chat_id': (None, chat),
            'caption': (None, caption),
            'parse_mode': (None, mode),
            'disable_notification': (None, silent),
            'document': (file, open(file, 'rb')),
        }
        url = "https://api.telegram.org/bot" + token + "/sendDocument"
        r = post(url, files=files)
    else:
        print("Error!")
    status = r.status_code
    response = r.reason


def req_status():
    if out == 'yes':
        if status == 200:
            print("Message sent")
        elif status == 400:
            print("Bad recipient / Wrong text format")
        elif status == 401:
            print("Wrong / Unauthorized token")
        else:
            print("Unknown error")
        print("Response: " + response)


arg_parse()
send_message()
req_status()
