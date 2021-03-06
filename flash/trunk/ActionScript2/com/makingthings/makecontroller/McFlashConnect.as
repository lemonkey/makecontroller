﻿/*********************************************************************************
 Copyright 2006 MakingThings

 Licensed under the Apache License, 
 Version 2.0 (the "License"); you may not use this file except in compliance 
 with the License. You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0 

 Unless required by applicable law or agreed to in writing, software distributed
 under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 CONDITIONS OF ANY KIND, either express or implied. See the License for
 the specific language governing permissions and limitations under the License.
 
*********************************************************************************/

import com.makingthings.makecontroller.OscMessage;
import com.makingthings.makecontroller.AddressHandler;
import com.makingthings.makecontroller.Board;
import mx.utils.Delegate;

/** 
	<p>Mchelper (Make Controller Helper) is a separate application that must be run simultaneously with your Flash application.
	Because Flash can't connect to external devices by itself, Mchelper is a necessary intermediate step - 
	it can connect to the Make Controller over the network and via USB, and then it formats communication with the board
	into XML that can be fed in and out of Flash.  <b>McFlashConnect</b> makes a connection to Mchelper from your Flash movie.</p>
	
	<p>The general idea is that you:
		<ol>
			<li>Connect to mchelper.</li>
			<li>Receive info about which boards are connected.</li>
			<li>Send and/or receive messages to any of those boards.</li>
			<li>Receive info when mchelper has disconnected.</li>
		</ol>
	</p>
	
	<p>McFlashConnect emits a few events of interest - check these out at in the in the 
	<a href="McEvent.html">McEvent</a> section.</p>
	
	<p>The easiest way to get started is to check out a few of the example files included in the MakingThings download, and start
	modifying things to fit your project.</p>
*/

class com.makingthings.makecontroller.McFlashConnect 
{
	/** The IP address of the machine running Mchelper. 
		<p>Since this usually on the same machine you're running your Flash app on, 
		it's usually best to set this to <b>localhost</b></p> */
	public var mchelperAddress;
	/** The port that Mchelper is listening for connections from McFlashConnect on. Default is <b>11000</b>.*/
	public var mchelperPort;
	private var defaultBoard:Board;
	private var registeredAddresses:Array; // list of registered addresses and functions to call if a message with that address shows up
	private var connectedBoards:Array;
	private var mySocket:XMLSocket;
	public var connected:Boolean; 
	
  /**
		Create a new instance of McFlashConnect.
		*/
  function McFlashConnect( )
  {
		connected = false;
		mchelperAddress = "localhost";
		mchelperPort = 11000;
		registeredAddresses = new Array( );
		connectedBoards = new Array( );
  }
	
	/**
	Register a handler for messages with a given OSC address.
	
	<p>You can create a function of your own that will get called whenever an incoming OSC message matches
	a particular address.  This saves you from checking the address of all incoming OSC messages. </p>
	
	<p>Your callback should be in the form:
	<pre> myCallback( msg:OscMessage ); </pre></p>
	
	<h3>Example</h3>
	<pre>
	mcflash.addAddressListener( "/analogin/7/value", onTrimpot );
	
	// this will get called whenever a message from "/analogin/7/value" is received.
	function onTrimpot( msg:OscMessage ) 
	{
		trace( "new OSC message: " + msg.toString() );
	}
	</pre>
	
	@param addr The OSC address to match.
	@param callback The function that you want to be called back on.
	*/
	public function addAddressListener( addr:String, callback:Function )
	{
		var handler = new AddressHandler( addr, callback );
		registeredAddresses.push( handler );
	}
	
	/**
	Set the board to send messages to when the send( ) method is used.  If no default board
	is set, you need to specify which board to send to using sendToBoard( ). It can be 

	convenient to set this when a new board arrives.
	
	<h3>Example</h3>
	<pre>
	function onBoardArrived( event:McEvent )
	{
		var newBoard:Board == event.data;
		mcFlash.setDefaultBoard( newBoard );
	}
	</pre>
	
	@param board A <b>Board</b> object indicating which board to send to.
	*/
	public function setDefaultBoard( board:Board ) : Void
	{
		defaultBoard = board;
	}
	
	/**
	From the list of connected boards, find a board with a given IP address.
	
	<h3>Example</h3>
	<pre>
	// find a board with the IP address 192.168.0.200
	var myBoard:Board = getBoardByIPAddress( "192.168.0.200" );
	</pre>
	
	@param address A string specifying the IP address of the board to find.
	@return The <b>Board</b> object with the given IP address, or null if it wasn't found.
	*/
	public function getBoardByIPAddress( address:String ) : Board
	{
		var boardCount:Number = connectedBoards.length;
		for( var i = 0; i < boardCount; i++ )
		{
			if( connectedBoards[ i ].location == address )
				return connectedBoards[ i ];
		}
		return null;
	}
	
	/**
	From the list of connected boards, find a board at a given USB location.  On Windows,
	this will be a COM port, otherwise it will be the device location.
	
	<h3>Example</h3>
	<pre>
	// find a board with the USB location COM9
	var myBoard:Board = getBoardByUsbLocation( "COM9" );
	</pre>
	
	@param location A string specifying the USB location of the board to find.
	@return The <b>Board</b> object with the given USB location, or null if it wasn't found.
	*/
	public function getBoardByUsbLocation( location:String ) : Board
	{
		var boardCount:Number = connectedBoards.length;
		for( var i = 0; i < boardCount; i++ )
		{
			if( connectedBoards[ i ].location == location )
				return connectedBoards[ i ];
		}
		return null;
	}
	
	/**
	From the list of connected boards, find all boards with a given name.  Because many
	boards can potentially have the same name, this will return an Array of <b>Board</b> objects
	with the given name.
	
	<h3>Example</h3>
	<pre>
	// find all boards named "Make Controller Kit"
	var myBoards:Array = getBoardsByName( "Make Controller Kit" );
	</pre>
	
	@param name A string specifying the name of the board to find.
	@return An Array with all the boards with the given name.  If no boards were found, this Array will be empty.
	*/
	public function getBoardsByName( name:String ) : Array
	{
		var boardCount:Number = connectedBoards.length;
		var boardArray:Array= [];
		for( var i = 0; i < boardCount; i++ )
		{
			if( connectedBoards[ i ].name == name )
				boardArray.push( connectedBoards[ i ] );
		}
		return boardArray;
	}
	
	/**
	From the list of connected boards, find all boards with a given serial number.  Because many
	boards can potentially have the same serial number, this will return an Array of <b>Board</b> objects
	with the given number.
	
	<h3>Example</h3>
	<pre>
	// find all boards with the serial number 18934
	var myBoards:Array = getBoardsBySerialNumber( 18934 );
	</pre>
	
	@param serialnum A Number specifying the serial number of the board to find.
	@return An Array with all the boards with the given serial number.  If no boards were found, this Array will be empty.
	*/
	public function getBoardsBySerialNumber( serialnum:Number ) : Array
	{
		var boardCount:Number = connectedBoards.length;
		var boardArray:Array= [];
		for( var i = 0; i < boardCount; i++ )
		{
			if( connectedBoards[ i ].serialNumber == serialnum )
				boardArray.push( connectedBoards[ i ] );
		}
		return boardArray;
	}
	
	
	// ************************************************************************************************************************************
	// OSC stuff
	// ************************************************************************************************************************************
	
	// ** parse messages from an XML-encoded OSC packet
	private function parseMessages( node:XMLNode ) : Void //this is called when we get OSC packets back from FLOSC.
	{	
		var time:Number = node.attributes.TIME;
		var addr:String = node.attributes.ADDRESS;
		var message:XMLNode = node.firstChild;
		while( message != null )
		{
			if (message.nodeName == "MESSAGE")
			{
				var msgName:String = message.attributes.NAME;		
				var oscData:Array = [];
				for (var child:XMLNode = message.firstChild; child != null; child=child.nextSibling)
				{
					if (child.nodeName == "ARGUMENT")
					{
						var type:String = child.attributes.TYPE;
						//float
						if (type=="f") {
							oscData.push(parseFloat(child.attributes.VALUE));
						} else 
						// int
						if (type=="i") {
							oscData.push(parseInt(child.attributes.VALUE));
						} else 
						//string
						if (type=="s") {
							oscData.push(child.attributes.VALUE);
						}	else 
						//string
						if (type=="b") {
							// we expect to receive pairs of 4-bit values representing the binary data
							// as a string of chars from 0-f
							var rawarray = new Array( ).concat( child.attributes.VALUE.split( "" ) );
							var realarray = new Array( );
							while( rawarray.length )
							{
								var val:Number = parseInt( rawarray.shift( ), 16 );
								val <<= 4;
								val += parseInt( rawarray.shift( ), 16 );
								realarray.push( val );
							}
							oscData.push( realarray );
						}	
						//trace( "Address: " + node.attributes.NAME + ", Arg: " + child.attributes.VALUE );
					}
				}
				var msg:OscMessage = new OscMessage( msgName, oscData );
				msg.from = addr;
				msg.time = time;
				//trace( "name: " + msg.address + ", arg: " + msg.args[0] + ", from: " + msg.fromIpAddress );
				doCallback( msg );
				message = message.nextSibling; // move to the next MESSAGE node, if there is one
			}
		}
	}
	
	private function doCallback( msg:OscMessage ) : Void
	{
		var knownAddresses:Number = registeredAddresses.length;
		var calledBack:Boolean = false;
		for( var i = 0; i < knownAddresses; i++ )
		{
			if( registeredAddresses[i].address == msg.address )
			{
				registeredAddresses[i].callback( msg );
				calledBack = true;
			}
		}
		if( !calledBack )
			onMessageIn( msg );
	}
	
	/**
	Get called back on this function with any incoming OSC messages.
	
	<p>Whenever a new OSC message arrives, this function will be called, and you can process the message
	however you like.</p>
	<p>Most often, you'll want to test the address of the incoming message to see if it's something you're interested in.
	So if you want to listen for the trimpot (analogin 7), for exmaple, you might implement it like...</p>
	
	<h3>Example</h3>
	<pre> 	
	mcflash.onMessageIn = function( msg:OscMessage )
	{
		if( msg.address == "/analogin/7/value" )
			var trimpot = msg.args[0]; // the value is in the first argument of the message
	}
	</pre>
	<p>Now the <b>trimpot</b> variable holds the value of the trimpot and you can do whatever you like with it. </p>
	<p>See the <a href="OscMessage.html">OscMessage</a> class for more details.</p>
 	*/
	public function onMessageIn( msg:OscMessage )
	{
	}
	
	/**
	Get notified when a new board is connected.
	
	<p>When a new board is connected, you may want to set it as the default board so that you don't have to store
	a reference to it yourself.  You might want to check its <b>type</b> first.</p>
	
	<h3>Example</h3>
	<pre> 	
	mcflash.onBoardArrived = function( board:Board )
	{
		if( board.type == "USB" )
			mcflash.setDefaultBoard( board );
	}
	</pre>
	
	<p>Or you might want to make sure it has a certain address...</p>
	
	<pre> 	
	mcflash.onBoardArrived = function( board:Board )
	{
		if( board.location == "192.168.0.121" )
			mcflash.setDefaultBoard( board );
	}
	</pre>
	*/
	public function onBoardArrived( board:Board )
	{
		//trace( "New board connected: " + board.location );
	}
	
	/** 
	Get notified when a board is disconnected.
	
	<h3>Example</h3>
	<pre> 	
	mcflash.onBoardRemoved = function( board:Board )
	{
		trace( "Board disconnected at " + board.location );
	}
	</pre>
	*/
	public function onBoardRemoved( board:Board )
	{
		//trace( "Board removed: " + board.location );
	}
	
	/** 
	Get notified when McFlashConnect could not connect to mchelper.
	
	<h3>Example</h3>
	<pre> 	
	mcflash.onConnectError = function( )
	{
		trace( "Failed to connect to mchelper." );
	}
	</pre>
	*/
	public function onConnectError( )
	{
		
	}
	
	/** 
	Get notified when McFlashConnect has connected successfully to mchelper.
	
	<h3>Example</h3>
	<pre> 	
	mcflash.onConnect = function( )
	{
		trace( "Successfully connected to mchelper." );
	}
	</pre>
	*/
	public function onConnect( )
	{
		
	}
	
	/**
	Connect McFlashConnect up to Mchelper.
	This will connect using the current values of <b>mchelperAddress</b> and <b>mchelperPort</b>.
	
	<h3>Example</h3>
	<pre> 
	mcflash.connect( );
	</pre>
	*/
	public function connect( )
	{
		mySocket = new XMLSocket();
		mySocket.onConnect = Delegate.create( this, handleConnect );
		mySocket.onClose = Delegate.create( this, handleClose );
		mySocket.onXML = Delegate.create( this, handleIncoming );
	
		if (!mySocket.connect(mchelperAddress, mchelperPort))
			onConnectError( );
	}
	
	/**
	Disconnect from mchelper.
	
	<p>This closes the XML connection to mchelper.  This does not need to be called explicitly before closing your movie.</p>
	
	<h3>Example</h3>
	<pre> 
	mcflash.disconnect( );
	</pre>
	*/
	public function disconnect( ) 
	{
		if( connected )
		{
			mySocket.close();
			connected = false;
		}
		else
			return;
	}
	
	private function handleClose( )
	{
		//trace( "XML socket closed" );
	}
	
	// *** event handler for incoming XML-encoded OSC packets
  private function handleIncoming (xmlIn)
	{
		// parse out the packet information
		//trace( "new xml" );
		var xmlDoc:XML = new XML( );
		xmlDoc.ignoreWhite = true;
		xmlDoc.parseXML( xmlIn );
		var n:XMLNode = xmlDoc.firstChild;
		//trace( xmlDoc );
		if( n != null )
		{
			if( n.nodeName == "OSCPACKET" )
				parseMessages( n );
			else if( n.nodeName == "BOARD_ARRIVAL" || n.nodeName == "BOARD_REMOVAL" )
				updateBoardList( n );
			else if( n.nodeName == "BOARD_INFO" )
				updateBoardInfo( n );
		}
	}
	
	private function updateBoardList( xml:XMLNode ):Void
	{
		var boardMessage:XMLNode = xml.firstChild;
		if( xml.nodeName == "BOARD_ARRIVAL" )
		{
			while( boardMessage != null )
			{
				var newBoard:Board = new Board( boardMessage.attributes.TYPE, boardMessage.attributes.LOCATION );
				if( boardMessage.attributes.NAME )
					newBoard.name = boardMessage.attributes.NAME;
				if( boardMessage.attributes.SERIALNUMBER )
					newBoard.serialnumber = int( boardMessage.attributes.SERIALNUMBER );
				connectedBoards.push( newBoard );
				onBoardArrived( newBoard );
				boardMessage = boardMessage.nextSibling;
			}
		}
		else if( xml.nodeName == "BOARD_REMOVAL" )
		{
			while( boardMessage != null )
			{
				for( var i = 0; i < connectedBoards.length; i++ )
				{
					if( connectedBoards[i].location == boardMessage.attributes.LOCATION )
					{
						var newBoard:Board = connectedBoards[i];
						connectedBoards.splice( i, 1 );
						onBoardRemoved( newBoard );
					}
				}
				boardMessage = boardMessage.nextSibling;
			}
		}
	}
	
	private function updateBoardInfo( xml:XMLNode ) : Void
	{
		var boardMessage:XMLNode = xml.firstChild;
		while( boardMessage != null )
		{
			if( boardMessage.nodeName == "BOARD" )
			{
				for( var i = 0; i < connectedBoards.length; i++ )
				{
					var board:Board = connectedBoards[i];
					if( board.location == boardMessage.attributes.LOCATION )
					{
						board.name = boardMessage.attributes.NAME;
						board.serialnumber = int( boardMessage.attributes.SERIALNUMBER );
					}
				}
			}
			boardMessage = boardMessage.nextSibling;
		}
	}
	
	// *** event handler to respond to successful connection attempt
	private function handleConnect (succeeded)
	{
		if( succeeded == true )
		{
			this.connected = true;
			onConnect( );
		}
		else
			onConnectError( );
	}
	
	/**
	* Send a message to the board.
	<p>This will send a message to the current <b>defaultBoard</b>, as set by setDefaultBoard( ).
	If you need to specify the board to send to, use sendToBoard( ).</p>
	
	<h3>Example</h3>
	<pre> 
	// Specify the OSC address and argument to send - turn on LED 1
	mcflash.send( "/appled/1/state", [1] );
	
	// Or, to read the state of the LED...
	mcflash.send( "/appled/1/state", [] );
	</pre>
	
	@param address The OSC address to send to, as type \b String.
	@param arg The value to be sent.  Send an empty Array if there are no arguments.
	@see sendToBoard( )
	*/
	public function send( address:String, args:Array )
	{
		sendToBoard( address, args, defaultBoard );
	}
	
	/**
	* Send a message to a given board. 
	
	<h3>Example</h3>
	<pre> 
	// Specify the OSC address and argument to send - turn on LED 1
	var myBoard:Board = mcflash.getBoardByIPAddress( "192.168.0.200" );
	mcflash.sendToBoard( "/appled/1/state", [1], myBoard );
	</pre>
	
	@param address The OSC address of your message.
	@param args The value(s) to be sent.
	@param board The board to send to.
	*/
	public function sendToBoard( address:String, args:Array, board:Board )
	{
		if( board == null )
			return;
		
		var xmlOut:XML = new XML();
		var packetOut = createPacketOut( xmlOut, 0, board.location );
		var xmlMessage = createMessage( xmlOut, packetOut, address );
		parseArguments( xmlOut, xmlMessage, args );
		
		xmlOut.appendChild(packetOut);
	
		if( mySocket && this.connected )
			mySocket.send(xmlOut);
	}
	
	// used internally to prep an XML object to be sent out.
	private function createPacketOut( xmlOut:XML, time:Number, destination:String ):XMLNode
	{
		var packetOut = xmlOut.createElement("OSCPACKET");
		packetOut.attributes.TIME = 0;
		packetOut.attributes.PORT = 0;
		packetOut.attributes.ADDRESS = destination;
		
		return packetOut;
	}
	
	// used internally to create a message element within the xmlOut object
	private function createMessage( xmlOut:XML, packetOut:XMLNode, address:String ):XMLNode
	{
		var xmlMessage = xmlOut.createElement("MESSAGE");
		xmlMessage.attributes.NAME = address;
		packetOut.appendChild(xmlMessage);
		return xmlMessage;
	}
	
	// used internally to determine the type of an argument, and append it to its corresponding message in the outgoing XML object.
	private function parseArguments( xmlOut:XML, xmlMessage:XMLNode, args:Array )
	{
		var argument:XMLNode;
		var argsLength:Number = args.length;
		// NOTE : the server expects all strings to be encoded with the escape function.
		for( var i = 0; i < argsLength; i++ )
		{
			argument = xmlOut.createElement("ARGUMENT");
			var argInt = parseInt(args[i]);
			if(isNaN(argInt))
			{
				argument.attributes.TYPE = "s";
				argument.attributes.VALUE = escape(args[i]);
			} 
			else
			{
				var argString:String = args[i].toString();
				var stringLength:Number = argString.length;
				var float:Boolean = false;
				for( var j = 0; j < stringLength; j++ )
				{
					if( argString.charAt( j ) == "." )
					{
						argument.attributes.TYPE="f";
						argument.attributes.VALUE=parseFloat(argString);
						float = true;
						break;
					}
				}
				if( !float )
				{
					argument.attributes.TYPE="i";
					argument.attributes.VALUE=parseInt(args[i]);
				}
			}
			xmlMessage.appendChild(argument);
		}
	}
	
	/**
	* Send an <b>OscMessage</b> to a board. 
	<p>This will send a message, of type <b>OscMessage</b>, to the current <b>defaultBoard</b>.
	If you need to specify another board to send to, use sendMessageToBoard( ). </p>
	
	<h3>Example</h3>
	<pre> 
	// create an OscMessage
	var turnOnLed:OscMessage = new OscMessage( "/appled/0/state", [1] );
	// now send it
	mcflash.sendMessage( turnOnLed );
	</pre>
	
	@param oscM The message, of type <b>OscMessage</b>, to be sent
	@see sendMessageToBoard( )
	*/
	public function sendMessage( oscM:OscMessage )
	{
		sendMessageToBoard( oscM, defaultBoard );
	}
	
	/**
	* Send an OscMessage to a given board. 
	
	<h3>Example</h3>
	<pre> 
	// create an OscMessage
	var turnOffLed:OscMessage = new OscMessage( "/appled/0/state", [0] );
	// now send it to a specified address
	var myBoard:Board = mcflash.getBoardByIPAddress( "192.168.0.200" );
	mcflash.sendMessageToAddress( turnOffLed, myBoard );
	</pre>
	
	@param oscM The message, of type OscMessage, to be sent
	@param board The board to send to.
	*/
	public function sendMessageToBoard( oscM:OscMessage, board:Board )
	{
		if( board == null )
			return;	

		var xmlOut:XML = new XML();
		var packetOut = createPacketOut( xmlOut, 0, board.location );
		var xmlMessage = createMessage( xmlOut, packetOut, oscM.address );
		parseArguments( xmlOut, xmlMessage, oscM.args );

		xmlOut.appendChild(packetOut);
	
		if( mySocket && this.connected )
			mySocket.send(xmlOut);
	}
	
	/**
	Send an OscBundle to the board.
	<p>This will send a bundle to the current <b>defaultBoard</b>.
	If you need to specify another board to send to, use sendBundleToBoard( ). </p>
	
	<p>It's a good idea to send bundles when possible, in order to reduce the traffic between the board and Flash.
	If you need to specify the address and port for each message, use sendBundleToAddress( ).</p>
	
	<h3>Example</h3>
	<pre>
	// create an Array (this Array will be our OscBundle)
	// and stuff our messages into it
	var myOscBundle:Array = new Array( );
	
	// create a couple of OscMessages
	myOscBundle.push( new OscMessage( "/appled/0/state", [0] ) ); // turn off LED 0
	myOscBundle.push( new OscMessage( "/analogin/0/value", [] ) ); // ask for analogin 0

	// now send it
	mcflash.sendBundle( myOscBundle );
	</pre>
	
	@param oscB The OscBundle to be sent
	@see sendBundleToBoard( ) 
	*/
	public function sendBundle( oscB:Array )
	{
		sendBundleToBoard( oscB, defaultBoard );
	}
	
	/**
	Send an OscBundle to a given board. 
	
	<h3>Example</h3>
	<pre> 
	// create an Array (this Array will be our OscBundle)
	// and stuff our messages into it
	var myOscBundle:Array = new Array( );
	
	// create a couple of OscMessages
	myOscBundle.push( new OscMessage( "/appled/0/state", [0] ) ); // turn off LED 0
	myOscBundle.push( new OscMessage( "/analogin/0/value", [] ) ); // ask for analogin 0
	
	// now send it to a specified board
	var myBoard:Board = mcflash.getBoardByIPAddress( "192.168.0.200" );
	mcflash.sendBundleToAddress( myOscBundle, myBoard );
	</pre>
	
	@param oscB The OscBundle to be sent.
	@param board The board to send to.
	*/
	public function sendBundleToBoard( oscB:Array, board:Board )
	{
		if( board == null )
			return;
				
		var xmlOut:XML = new XML();
		var packetOut = createPacketOut( xmlOut, 0, board.location );
		for( var i = 0; i < oscB.length; i++ )
		{
			if( oscB[i] instanceof OscMessage == false )
			{
				trace( "Error - Item #" + i + " in the OscBundle was not an OscMessage...the entire bundle was not sent." );
				return;
			}

			var oscM:OscMessage = oscB[i];
			//trace( "Message " + i + ", address " + oscM.address + ", arg: " + oscM.args[0] );
			var xmlMessage = createMessage( xmlOut, packetOut, oscM.address );
			parseArguments( xmlOut, xmlMessage, oscM.args );
		}

		xmlOut.appendChild(packetOut);
	
		if( mySocket && this.connected )
			mySocket.send(xmlOut);
	}
}





