# configfix


*configfix* is a tool to resolve conflicts for configuration options in the Linux kernel that have unmet dependencies. It is built as an extension of xconfig.

![Preview1](images/configfix_gen1.png) ![Preview2](images/configfix_gen2.png)

----

## Background

The vast majority of the configuration options in the Linux kernel have some kind of dependency relationship to one or several other options. An option cannot be configured as long as its dependencies are not satisfied. Consequently, these dependencies have to be taken into account when the user wants to configure an option.

While the various kernel configurators (xconfig, gconfig, menuconfig etc.) can display existing dependencies, they do not show whether all dependencies are met and, more importantly, which configuration changes must be made in order to be able to configure (i.e., select/deselect) a given configuration option.  If a user wants to enable option X, they will need to spend a lot of time understanding all the existing dependencies and figure out how to satisfy them.

We have therefore developed a conflict-resolution algorithm integrated right within *xconfig*. In this context, a conflict is when the selection/deselection of a kernel option violates an existing Kconfig dependency. The user can specify a configuration options and the value they wish to set this option to. configfix will then suggest a list of configuration options to be changed in order to satisfy the dependencies.


## Prerequisites

As configfix is integrated within xconfig, it needs xconfig to run as well as the [Glib library](https://developer.gnome.org/glib/stable/glib-data-types.html).

*  For xconfig:

    *  For Debian/Ubuntu: `apt install pkg-config g++ qtbase5-dev`

*  For the Glib library:

    *  For Debian/Ubuntu: `apt install libglib2.0-dev`

## Instructions

1. Clone this repository.
2. Download the kernel source tree for version 5.3 from [the official archives](https://cdn.kernel.org/pub/linux/kernel/v5.x/) and extract it.
3. Run `copy_files.sh $KERNEL_SOURCE_TREE` in this repository.
4. Run `make xconfig` in your extracted source tree.


If the bottom panel is invisible, you need to drag it up. See the following screenshots.

![Hidden1](images/hidden1.png) ![Hidden2](images/hidden2.png)


## Limitations

* Some conflicts can be fixed using various alternative fixes. We currently limit the number of proposed fixes to 3 for performance reasons. While every proposed fix should still result in the desired outcome, the solutions can be suboptimal in some cases. For example, an option can be enabled while still being invisible in the configurator.
* The tool is somewhat memory-hungry. Currently, it needs about 1 GB of RAM to resolve conflicts, but we are working on reducing the memory consumption as well as improving the performance.
* The *Apply Fix*-button will try to apply all values from the selected fix. Sometimes it is not possible to set all values. In this case, please report the issue (see below).


## Contributing

Contributions to the project are appreciated. You can help, even if you are not a developer/programmer:

1. If you come across a conflict that cannot be solved or where configfix produces a wrong solution, please report the issue.
2. Merge requests to improve the code/performance or to fix bugs are highly appreciated as well.


## Credits

* Patrick Franz (University of Gothenburg) `<patrick.franz@gu.se>`
* Ibrahim Fayaz (VecScan AB) `<phayax@gmail.com>`
* Thorsten Berger (Chalmers | University of Gothenburg) `<thorsten.berger@chalmers.se>`
* Sarah Nadi (University of Alberta) `<nadi@ualberta.ca>`
