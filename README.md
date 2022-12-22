# Discord Rich Presence for Notepad++

![GitHub](https://img.shields.io/github/license/Zukaritasu/notepadpp_rpc) ![GitHub all releases](https://img.shields.io/github/downloads/Zukaritasu/notepadpp_rpc/total) ![GitHub release (latest by date)](https://img.shields.io/github/v/release/Zukaritasu/notepadpp_rpc)

This plugin shows Discord the file you are currently editing in [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus). If you change to another file or open a file the plugin will automatically show the new file you are editing. When you close [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) or disable rich presence the plugin will stop showing in discord what you are editing until you enable the plugin

The plugin has an options window in which you can edit the way the plugin should display the file you are editing, you also have the possibility to create your own application to use your custom images in the rich presence. For more information: [Create Custom Rich Presence](https://github.com/Zukaritasu/notepadpp_rpc/blob/main/DOCUMENTATION.md)

## Installation

To install the plugin manually you must download the plugin from <[Releases](https://github.com/Zukaritasu/notepadpp_rpc/releases)>, then you must create a folder with the name "**DiscordRPC**" in the following path:
 * **C:\Program Files\Notepad++\plugins** or
 * **C:\Program Files (x86)\Notepad++\plugins**

After creating the folder you must unzip the downloaded file and paste it into the following path:
 * **C:\Program Files\Notepad++\plugins\DiscordRPC**

And now it's time! Open Notepad++ and verify that the rich presence is showing up in Discord. Remember that the plugin requires internet connection and to have Discord open, although you can continue editing your files without any problem in case you don't have internet connection or Discord is closed

![](./sample_rpc.png)

*The plugin can currently be found in [The official collection of Notepad++ plugins](https://github.com/notepad-plus-plus/nppPluginList) so by simply opening the plugin manager from Notepad++ you can install it by entering "Discord Rich Presence" in the search box*

*The plugin has a default application ID, so it is not necessary to create an application for the plugin to work, only in case you want to create a custom application with your images*

## Support
If you have any problems with the plugin or have any questions about its functionality, you can join my discord server https://discord.gg/xEtAtmU6 and go to the **#npp-support** channel and leave your question with the following information:

```
- What version of Windows do you have?
- 32 or 64 bits architecture?
- The version of Notepad++ you have
- The DiscordRPC plugin version
```

## Documentation

* [Create Custom Rich Presence](https://github.com/Zukaritasu/notepadpp_rpc/blob/main/DOCUMENTATION.md)

## Discord SDK Information

* [SDK Starter Guide](https://discord.com/developers/docs/game-sdk/sdk-starter-guide)
* [Activities](https://discord.com/developers/docs/game-sdk/activities)
* [How To](https://discord.com/developers/docs/rich-presence/how-to)
