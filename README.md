# marss
Tool to download new (audio) episodes of RSS feeds and store them in a folder. It can then be played back as an audiobook.

## syntax

Running marss without any arguments just displays the copyright and syntax information.

Use the argument -d to download new episodes of the configured RSS feeds.


## Configuration

Create a file called marss.conf in your <HOME_FOLDER/.marss>. The format of this file is provided in marss.conf.sample.

If "prepend feed number" is "true", the downloaded files will be prefixed with the feed number (for example if the name of the file - downloaded from 3rd feed in the rss was episode183.mp3, the file will be saved as 03_episode183.mp3). It is recommended this not be set to "false" in order to avoid random ordering of downloaded episodes and to avoid duplicate file names being renamed by the application.

There is an option to create subfolders with current date to place the downloaded episodes. The format of the subfolder name will be YYYYMMDD.

The feeds will be numbered in the order that they appear in the configuration file, and not dependent on the numbers given to the feeds. You can re-arrange the feed sections in order to control the order of the downloaded episodes. If you want marss to rename the sections in the order in which they appear, set "reorder feed names" to "true". The feeds will be renamed and the option will be reset to "false". If the current feed names are "Feed01", "Feed02" etc, the feeds will be renumbered as "Feed001", "Feed002" etc. Otherwise, they will be renumbered as "Feed01", "Feed02", etc.

In the [Feed] section, "Select=false" will download ALL new episodes. "Select=true" will offer a list of new episodes and allow you to select which episodes you want to download.

When adding a new feed, just add Title, URL and Select fields. Other fields will be added by the application as new episodes are downloaded. The first time a feed is processed, the application downloads any episodes that were released within the last one week from the time the application being run.
