# Sparkler: A KVM based virtual machine monitor
Welcome to Sparkler, a virtual machine monitor and a tiny "operating system" to go with it. When you start Sparkler, it creates a virtual machine using the Linux's KVM API. It is written in C and emulates the following devices:

- Console: This is the serial console via which the VM can read the keyboard and write to the screen
- Twitter device: Reads the latest tweet from [Command Line Magic's Twitter account](https://twitter.com/climagic)
- Weather device: Fetches the weather for a few cities
- Air Quality device: Fetches the air quality readings for a few cities

## Accompanying article
Placeholder for article link

## Sparkler architecture
![sparkler architecture](https://unixism.net/wp-content/uploads/2019/10/Sparkler-Architecture.png)

## Building
Please install GCC and NASM packages for your Linux distribution before you build. Change into the Sparkler directory and type "make". That's all there is to it.

## Running
Just run `./sparkler` and that should start a Sparkler VM. You can then play around with the options the VM presents. Some distributions need the user to be part of a `kvm` group if you want to run this as a regular user. Else just prefix the command with `sudo`.

## A sample Sparkler session
![Sparkler Session](https://unixism.net/wp-content/uploads/2019/10/Sparkler_screenshot.png)

## The Sparkler web service
Although we use `libcurl` to fetch content off the internet and use [`json-parser`](https://github.com/udp/json-parser) to parse JSON, doing this is a real pain from C. This is very apparent if you’re like me and you’ve for exposed to the simplicity of handling this kind of stuff with higher-level languages. And so, I wrote a quick-and-dirty Sparkler Web Service that outputs JSON that is easily parsable from C. Also, it lets you try out Sparkler in its full glory without you first having to register for a Twitter developer account for you to access the Twitter API, to be able to fetch the tweet. This NodeJS service runs on the excellent Heroku platform for free. You can check out some JSON it outputs by clicking on these links here:

[Tweet Service](href="https://sparkler-service.herokuapp.com/tweet)
[Weather Service](https://sparkler-service.herokuapp.com/weather)
[Air Quality Service](https://sparkler-service.herokuapp.com/air_quality)

As you can see, I’ve made output from these different APIs structurally similar while removing a whole lot of JSON data we’ll never use. This lets us handle this with C fairly easily. When the monitor program requests for information from the <code>sparkler</code> program, it makes a request to the web service, parses that information and returns it to the monitor program.
