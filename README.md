
Introduction to Freewheeling
----------------------------

![freewheeling screenshot][Freewheeling_Screenshot]

### Free and Open Source Audio Tool for Live Looping

Freewheeling provides a highly configurable, intuitive, and fluid user
interface for instrumentalists to capture audio loops in real-time. Best
way to understand Freewheeling?? [See and hear it][Freewheeling_Screenshots] in action.
[Demo Video][Freewheeling_Demo_Video].


### Philosophy

```
Freewheeling is
a new way 'to be'
In The Muse-ical Moment.
It's a live looping instrument
that returns us to the joy
of making music 'spontaneously'.
```

Freewheeling allows us to build repetitive grooves by sampling and
directing loops from within spirited improvisation. It works because,
down to the core, it's built around improv. We leave mice and menus, and
dive into our own process of making sound. The principle author of
Freewheeling is JP Mercury. Freewheeling was also originally a Max/MSP
external. Since 2016, it is maintained by the user community.


### Technical Details

Freewheeling runs under GNU/Linux (PC/Mac) and Mac OS X (PPC/Intel)
using the [Jack Audio Connection Kit][JACK] sound server, and
is Free and Open Source Software, released under the GNU GPL license.
The ChangeLog is a good resource, because all major new features have
been documented there. It is humble enough to run on modest hardware but
can take advantage of the gobs of RAM on modern machines as well. The
only limits are your imagination.

**Focus on the ideation process:**

-   Freewheeling empowers the trance of immediate creativity by bringing
    us into a circular process. Time is utterly relative, and we are
    freed from the future-oriented mindset of product sequencing. If
    inspiration flows, later arranging and editing on a timeline can be
    done with other tools.

See the [Technical Features][Technical_Features].


Getting Started
---------------

### Downloading

Pre-compiled Freewheeling binaries are available for several of today's
popular operating systems. If you prefer using your favorite graphical
package manager (such as synaptic) or your distro's Add/Remove Software
Control Panel, search for a package named *freewheeling* or *fweelin*.

-   Install one of the [Packages][OS_Specific_Packages] for your distro - (recommended)
-   or, [Download][Freewheeling_Tarballs] a tarball - (advanced)
-   or, Checkout the latest git sources - (advanced)

```
    $ git clone https://github.com/free-wheeling/freewheeling.git
    $ git clone https://pagure.io/freewheeling.git
```


### Installing

-   [OS Specific Packages][OS_Specific_Packages] - (recommended)
-   [Compiling][Compiling] - (advanced)
-   [Dependencies][Dependencies]


### Configuring

-   [Configuration Basics][Configuration_Basics] - how does it work?
-   [XML Configuration System][XML_Configuration_System] -
    the full Freewheeling XML API from config-help.txt in wiki form
-   [Configurations][Configurations] -
    a store for users to submit their custom interfaces, layouts, patches, etc


### Troubleshooting and Support

-   [FAQS][FAQS] - Freeq-wheely Asked Questions
-   [Known Issues][Known_Issues] - Resolutions to common
    pitfalls, snags, and glitches (notice i did not say bugs)
-   [Freewheeling Users Mailing List][Freewheeling_Users_Mailing_List] -
    If you're looking for help with Freewheeling,
    your question may already be answered there. If not, feel free to
    [join the mailing list][Freewheeling_Users_Mailing_List]
    and ask your question to the community
-   [Feature Requests][Freewheeling_Users_Mailing_List] -
    Please join the 'freewheeling-user' mailing list
-   [Bug Reporting][Github_Bug_Tracker] -
    Please reports bugs on the [GitHub issue tracker][Github_Bug_Tracker] or the
    [NotABug issue tracker][Notabug_Bug_Tracker]


### Using

-   [Quick Start Guide][Quick_Start_Guide] - handy defaults for the impatient user
-   [Freewheeling Video Tutorials][Freewheeling_Video_Tutorials] - watch
    freewheeling in action
-   [Controlling Freewheeling][Controlling_Freewheeling] - using external midi devices
-   [Default Key Bindings][Default_Key_Bindings] - what did I just pressed?
-   [Tips and Tricks for Studio and Live Use][Tips_and_Tricks_for_Studio_and_Live_Use]


NOTE: You will also need the JACK audio server and a way to configure it
(such as qJackCtl or Cadence). A very easy way to setup a GNU/Linux audio workstation
is by installing the [KxStudio Operating System][KxStudio_Downloads].

NOTE: Debian (and derrivatives such as Trisquel, Ubuntu, and Mint) can transform
their system into KxStudio simply by adding the [KxStudio repos][KxStudio_Repos]
and installing the 'kxstudio-meta-all' package (or 'kxstudio-meta-audio' for just the
pro-audio programs). It is highly recommended that you install at least
the 'kxstudio-default-settings' package which optimizes your system
performance for real-time audio use.

NOTE: ArchLinux (and derrivatives such as Parabola, Hyperbola, and Manjaro)
can simply install the 'pro-audio' package group, which includes Freewheeling.
It is highly recommended that you install at least the 'realtime-privileges' package
which configures your system for real-time audio use.


The Freewheeling Community
--------------------------

### Community Interaction

-   [Music made with Freewheeling][Music_made_with_Freewheeling]
-   [Freewheeling Users Mailing List][Freewheeling_Users_Mailing_List] -
    technical support, new releases, shared ideas and music

### How can I help?

-   Submit or edit wiki articles
-   Submit your interfaces and layouts for specific hardware to the
    [Configurations][Configurations] article
-   Submit screen-casts demonstrating your interfaces or the advanced
    features of freewheeling
-   Submit audio/video creations to the
    [Music made with Freewheeling][Music_made_with_Freewheeling] article


[Freewheeling_Screenshot]:                 http://freewheeling.sourceforge.net/flo-051-looptray-t.png
[Freewheeling_Screenshots]:                http://freewheeling.sourceforge.net/shots.shtml
[Freewheeling_Demo_Video]:                 http://freewheeling.sourceforge.net/video/fw-demo1.avi
[JACK]:                                    http://jackaudio.org
[Technical_Features]:                      https://github.com/free-wheeling/freewheeling/wiki/Technical-Features
[OS_Specific_Packages]:                    https://github.com/free-wheeling/freewheeling/wiki/OS-Specific-Packages
[Freewheeling_Tarballs]:                   https://github.com/free-wheeling/freewheeling/releases
[Compiling]:                               https://github.com/free-wheeling/freewheeling/wiki/Compiling
[Dependencies]:                            https://github.com/free-wheeling/freewheeling/wiki/Dependencies
[Configuration_Basics]:                    https://github.com/free-wheeling/freewheeling/wiki/Configuration-Basics
[XML_Configuration_System]:                https://github.com/free-wheeling/freewheeling/wiki/XML-Configuration-System
[Configurations]:                          https://github.com/free-wheeling/freewheeling/wiki/Configurations
[FAQS]:                                    https://github.com/free-wheeling/freewheeling/wiki/FAQS
[Known_Issues]:                            https://github.com/free-wheeling/freewheeling/wiki/Known-Issues
[Freewheeling_Users_Mailing_List]:         https://github.com/free-wheeling/freewheeling/wiki/Freewheeling-Users-Mailing-List
[Github_Bug_Tracker]:                      https://github.com/free-wheeling/freewheeling/issues
[Notabug_Bug_Tracker]:                     https://notabug.org/freewheeling/freewheeling/issues
[Quick_Start_Guide]:                       https://github.com/free-wheeling/freewheeling/wiki/Quick-Start-Guide
[Freewheeling_Video_Tutorials]:            https://github.com/free-wheeling/freewheeling/wiki/Freewheeling-Video-Tutorials
[Controlling_Freewheeling]:                https://github.com/free-wheeling/freewheeling/wiki/Controlling-Freewheeling
[Default_Key_Bindings]:                    https://github.com/free-wheeling/freewheeling/wiki/Default-Key-Bindings
[Tips_and_Tricks_for_Studio_and_Live_Use]: https://github.com/free-wheeling/freewheeling/wiki/Tips-and-Tricks-for-Studio-and-Live-Use
[KxStudio_Downloads]:                      https://kxstudio.linuxaudio.org/Downloads
[KxStudio_Repos]:                          https://kxstudio.linuxaudio.org/Repositories
[Music_made_with_Freewheeling]:            https://github.com/free-wheeling/freewheeling/wiki/Music-made-with-Freewheeling
