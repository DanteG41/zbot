# zbot

[![CodeQL](https://github.com/DanteG41/zbot/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/DanteG41/zbot/actions/workflows/codeql-analysis.yml)

Telegram bot for Zabbix alerts. zbot queues the messages Zabbix sends, groups similar ones
into a single readable message, and lets you manage Zabbix from a Telegram menu.

A burst of alerts about the same problem turns into one message instead of a wall of
notifications:

```
16 similar messages from 2026.09.12 17:36 to 20:48
⛔️ PROBLEM: 2026.09.12/hh:mm:ss: Queue … lag is too high (lag=?m ?s ?ms) on … Queue: … lag - ?m ?s ?ms … #zabbix https://zabbix.example.com/history.php?action=showgraph&itemids[]=10485
```
> Queue (orders, payments, invoices, refunds, product_prices, product_images, product_stock, search_index, recommendations, user_events, and 6 more)<br>
> on (db-main, db-replica)<br>
> (db-main.example.com, db-replica.example.com)

## Features

- **Aggregation.** Messages wait in a queue for a few seconds, then similar ones are sent as
  one message with a count, the time range and a template of what they have in common.
- **Readable templates.** Numbers, dates, durations and words that differ are masked each in
  their own way, and the values hidden behind the masks are listed in an expandable quote.
- **Merging with what was already sent.** A new group absorbs similar messages the bot sent to
  the chat a moment ago, so the chat keeps one message per problem.
- **Zabbix menu.** Create, extend and delete maintenance periods, enable and disable actions,
  acknowledge problems and view graphs without leaving Telegram.
- **Pausing.** Stop sending to one chat or to all of them from the menu.
- **Resilience.** Every request to Telegram has a deadline, unsent messages survive a restart,
  and a failing Zabbix or Telegram does not take the daemon down.

## Requirements

- Linux
- CMake 3.3 or newer
- A C++ compiler with C++14 support, such as GCC 10 or newer
- Boost 1.66 or newer: Asio, Property Tree, System
- OpenSSL
- zlib
- A Telegram bot token, see [@BotFather](https://t.me/BotFather)
- For the menu: Zabbix 4.0 or newer with its frontend reachable over HTTPS on port 443

[tgbot-cpp](https://github.com/DanteG41/tgbot-cpp) and
[simpleini](https://github.com/brofield/simpleini) come as git submodules.

## Installation

### Gentoo

An ebuild with an OpenRC service is in [misc/ebuild](misc/ebuild). Copy it into a local
overlay and install `net-im/zbot`. The ebuild expects the `acct-user/zbot` and
`acct-group/zbot` packages for the user and the group, and installs the sample configuration
as `/etc/zbot/zbot.ini`. Create the storage directory as shown below.

### From source

```bash
git clone --recursive https://github.com/DanteG41/zbot.git
cd zbot
mkdir build && cd build
cmake ..
make
sudo make install
```

This installs `zbotd` and `zbotcli` into `bin` and `libzbot.so` into `lib` under the
installation prefix. Run `sudo ldconfig` if the library is not found.

Create a user, the directories and the configuration:

```bash
sudo groupadd --system zbot
sudo useradd --system --gid zbot --home-dir /var/spool/zbot --shell /sbin/nologin zbot
sudo install -d -o zbot -g zbot -m 2770 /var/spool/zbot
sudo install -d -o zbot -g zbot -m 750 /var/run/zbot
sudo install -o zbot -g zbot -m 640 /dev/null /var/log/zbot.log
sudo install -D -o root -g zbot -m 640 misc/sample_config.ini /etc/zbot/zbot.ini
```

The storage directory must not give any access to others and must have neither the setuid
nor the sticky bit, zbot refuses to use it otherwise. The configuration holds the bot token
and the Zabbix password, so keep it readable for the `zbot` group only.

### Running

With OpenRC:

```bash
sudo rc-service zbotd start
sudo rc-service zbotd reload
```

Without it, start the daemon as the `zbot` user. It forks into the background and writes its
PID to `pid_file`:

```bash
sudo -u zbot zbotd /etc/zbot/zbot.ini
```

`SIGUSR1` reloads the sending and grouping parameters: `wait`, `accuracy`, `spread`,
`max_messages`, `min_approx`, `dont_approximate_multibyte`, `immediate_send` and the history
parameters. Any other change needs a restart.

```bash
sudo kill -USR1 "$(cat /var/run/zbot/zbotd.pid)"
```

## Usage

### zbotcli

```
zbotcli [-f configfile] -c chat -m message [-i]
```

| Option | Description |
|---|---|
| `-f` | Configuration file, `/etc/zbot/zbot.ini` by default |
| `-c` | Telegram chat ID, negative for groups |
| `-m` | Message text |
| `-i` | Send right away, skipping the queue and the grouping |

`zbotcli` puts the message into the queue in the storage directory, and `zbotd` sends it.
A user other than `zbot` has to be a member of the `zbot` group to write there:

```bash
sudo usermod -a -G zbot zabbix
```

### Connecting Zabbix

Create a script in the `AlertScriptsPath` directory of the Zabbix server, for example
`zbot.sh`:

```sh
#!/bin/sh
exec /usr/bin/zbotcli -c "$1" -m "$2"
```

Then add a media type of the **Script** type with the script name `zbot.sh` and two
parameters, `{ALERT.SENDTO}` and `{ALERT.MESSAGE}`. In the media of a user, **Send to** is the
Telegram chat ID. Find it with the bot menu: **Info**, then **Chat ID**.

Start every alert with a status emoji, such as `⛔️` for problems and `✅` for recoveries, and
set `dont_approximate_multibyte=1`. Messages with different emoji then never end up in the
same group.

### Telegram menu

Add the bot to a chat and send `/start`, or `/start@your_bot` in a group. Only the users
listed in `admin_users` get the menu, everyone else gets `Access denied.`

| Menu | What it does |
|---|---|
| Info | Shows the chat ID and the list of admin users |
| Maintenance | Lists maintenance periods, creates one for a host group, extends one by an hour or deletes it |
| Actions | Enables and disables Zabbix actions, pauses and resumes sending in this chat or in all chats |
| Screen | Shows the graphs of a screen, only on Zabbix older than 5.4 |
| Problems | Lists unacknowledged disaster problems by host group and acknowledges them with a comment |

While sending is paused, queued messages are dropped. Changes to actions and pauses are
reported to `notify_chats`.

The Zabbix user needs API access. Creating maintenance periods and changing actions require
a user of the Admin type with write permissions on the host groups involved.

## Message aggregation

Every `wait` seconds `zbotd` takes up to `max_messages` queued messages of each chat. When
there are more than `min_approx` of them, similar messages are grouped. Messages are compared
word by word. Numbers, dates and durations that differ do not count as a difference, words
do. Two messages go into one group when their difference is below `accuracy`.

A group of several messages is sent as a header with the count and the time the first and
the last of them were queued, followed by a template:

| In the template | Stands for |
|---|---|
| `?` | A number that differs, `7.42` and `31.07` give `?`, `26m` and `3m` give `?m` |
| `2026.09.1d/1h:mm:ss` | The digits of a date or a time that differ, named after their field |
| `?h ?m ?s` | A duration, with every unit any of the messages has |
| `…` | A word that differs or that only some of the messages have |

A link that differs is replaced with the link of the latest message. The words hidden behind
`…` are listed in an expandable quote, each line named after the word left of the masks, at
most ten values per line.

Before a group is sent, it is compared with the last `history_check_count` messages the bot
sent to the chat during `history_max_age_minutes`. Similar ones are merged into the group, the
group is sent, and the messages it replaces are deleted from the chat. This history lives in
memory and starts empty after a restart.

Messages longer than 200 words are never grouped.

## Configuration

The configuration is an INI file with the sections `[main]`, `[zabbix]` and `[telegram]`. A
parameter that is not set takes its default. See [misc/sample_config.ini](misc/sample_config.ini).

### [main]

| Parameter | Default | Description |
|---|---|---|
| `storage` | `/var/spool/zbot` | Directory of the message queue |
| `log_file` | `/var/log/zbot.log` | Log file |
| `pid_file` | `/var/run/zbot/zbotd.pid` | PID file of the daemon |
| `wait` | `10` | Seconds between two passes over the queue. The bot also waits this long before it retries a failed Zabbix login |
| `accuracy` | `0.4` | Two messages go into one group when their difference is below this value, from 0 for identical to 1 for nothing in common |
| `spread` | `0.1` | Groups whose difference from a message is within `spread` of the closest group count as equally close, the biggest of them gets the message |
| `max_messages` | `50` | Queued messages taken from a chat in one pass |
| `min_approx` | `5` | Messages are grouped only when a pass takes more than this many, a smaller batch is sent message by message |
| `bot_enable` | `1` | Run the Telegram menu. With `0` zbot only sends messages |
| `dont_approximate_multibyte` | `0` | Do not group messages if that would mask text with non-ASCII characters, such as emoji or Cyrillic words |
| `immediate_send` | `0` | Handle every queued message on its own: join it to a group sent recently, or build a group with single messages sent recently, instead of grouping the whole batch |
| `history_check_count` | `20` | How many of the latest messages sent to a chat a new group is compared with |
| `history_max_age_minutes` | `60` | How old those messages may be. In the default mode, `0` here or in `history_check_count` turns merging with sent messages off |

### [zabbix]

| Parameter | Default | Description |
|---|---|---|
| `zabbix_url` | `http://company.com/zabbix/` | Address of the Zabbix frontend, ending with a slash. zbot always connects over HTTPS on port 443 and does not verify the certificate |
| `user` | `zbot` | Zabbix user for the API and for downloading graphs |
| `password` | `zbotpassword` | Password of that user |

### [telegram]

| Parameter | Default | Description |
|---|---|---|
| `token` | | Bot token from @BotFather |
| `admin_users` | | Telegram usernames without `@` allowed to use the menu, separated by spaces, commas or semicolons |
| `notify_chats` | | Chat IDs that get a note when someone changes an action or pauses sending, separated the same way |
| `webhook_enable` | `0` | Receive updates through a webhook instead of long polling |
| `webhook_path` | `/zbot/webhook?token=randomhash` | Path of the webhook. Keep a random value in it |
| `webhook_public_host` | `zbot.org` | Host name Telegram delivers updates to, as `https://webhook_public_host` followed by `webhook_path` |
| `webhook_bind_port` | `8080` | Local port of the webhook server. It speaks plain HTTP, so put a reverse proxy with TLS in front of it |

## License

zbot is distributed under the GNU General Public License v3.0, see [LICENSE](LICENSE).
