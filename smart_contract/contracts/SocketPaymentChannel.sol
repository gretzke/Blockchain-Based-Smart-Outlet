pragma solidity ^0.5.0;

/**
 * @title SafeMath
 * @dev Unsigned math operations with safety checks that revert on error
 * @notice https://github.com/OpenZeppelin/openzeppelin-solidity
 */
library SafeMath {
    /**
     * @dev Multiplies two unsigned integers, reverts on overflow.
     */
    function mul(uint256 a, uint256 b) internal pure returns (uint256) {
        // Gas optimization: this is cheaper than requiring 'a' not being zero, but the
        // benefit is lost if 'b' is also tested.
        // See: https://github.com/OpenZeppelin/openzeppelin-solidity/pull/522
        if (a == 0) {
            return 0;
        }

        uint256 c = a * b;
        require(c / a == b);

        return c;
    }

    /**
     * @dev Integer division of two unsigned integers truncating the quotient, reverts on division by zero.
     */
    function div(uint256 a, uint256 b) internal pure returns (uint256) {
        // Solidity only automatically asserts when dividing by 0
        require(b > 0);
        uint256 c = a / b;
        // assert(a == b * c + a % b); // There is no case in which this doesn't hold

        return c;
    }

    /**
     * @dev Subtracts two unsigned integers, reverts on overflow (i.e. if subtrahend is greater than minuend).
     */
    function sub(uint256 a, uint256 b) internal pure returns (uint256) {
        require(b <= a);
        uint256 c = a - b;

        return c;
    }

    /**
     * @dev Adds two unsigned integers, reverts on overflow.
     */
    function add(uint256 a, uint256 b) internal pure returns (uint256) {
        uint256 c = a + b;
        require(c >= a);

        return c;
    }

    /**
     * @dev Divides two unsigned integers and returns the remainder (unsigned integer modulo),
     * reverts when dividing by zero.
     */
    function mod(uint256 a, uint256 b) internal pure returns (uint256) {
        require(b != 0);
        return a % b;
    }
}

contract SocketPaymentChannel {
    using SafeMath for uint256;

    address public owner;
    uint256 public pricePerSecond;
    mapping(address => uint256) public balances;
    // timeout in case no one closes the payment channel in seconds
    uint256 public expirationDuration;
    uint256 public minDeposit;

    // global payment channel variables
    bool public channelActive;
    uint256 public creationTimeStamp;
    uint256 public expirationDate;
    address public channelCustomer;
    uint256 public maxValue;

    // use nonces to avoid replay attacks
    mapping(address => uint256) public customerNonces;

    event InitializedPaymentChannel(address indexed customer, uint256 indexed start, uint256 indexed maxValue ,uint256 end);
    event ClosedPaymentChannel(address indexed sender, uint256 indexed value, bool indexed expired, uint256 duration);
    event PriceChanged(uint256 indexed oldPrice, uint256 indexed newPrice);
    event Withdrawal(address indexed sender, uint256 indexed amount);

    modifier onlyOwner() {
        require(msg.sender == owner, "sender is not owner");
        _;
    }

    constructor(uint256 _pricePerSecond, uint256 _expirationDuration, uint256 _minDeposit) public {
        owner = msg.sender;
        pricePerSecond = _pricePerSecond;
        expirationDuration = _expirationDuration;
        minDeposit = _minDeposit;
        channelActive = false;
    }

    function initializePaymentChannel() public payable returns (bool) {
        require(!channelActive, "payment channel already active");
        require(msg.value >= minDeposit, "minimum deposit value not reached");

        // set global payment channel information
        channelActive = true;
        channelCustomer = msg.sender;
        maxValue = msg.value;
        creationTimeStamp = now;
        expirationDate = now.add(expirationDuration);

        // It's cheaper to use msg.sender instead of channelCustomer, msg.value instead of maxValue, etc.
        emit InitializedPaymentChannel(msg.sender, now, now.add(expirationDuration), msg.value);
        return true;
    }

    function closeChannel(bytes memory _signature, uint256 _value, uint256 _nonce) public onlyOwner returns (bool) {
        require(channelActive, "payment channel not active");
        // TODO check nonce before payout
        require(verifySignature(_signature, _value, _nonce), "signature not valid");

        // increase nonce after payment channel is closed
        customerNonces[channelCustomer] = customerNonces[channelCustomer].add(1);

        // if maxValue is exceeded, set value to maxValue
        if (_value > maxValue) {
            _value = maxValue;
            balances[owner] = balances[owner].add(_value);
        } else {
            balances[owner] = balances[owner].add(_value);
            balances[channelCustomer] = balances[channelCustomer].add(maxValue.sub(_value))
        }

        emit ClosedPaymentChannel(msg.sender, _value, false, now - creationTimeStamp);

        channelActive = false;
        channelCustomer = 0;
        maxValue = 0;
        expirationDate = 0;
        creationTimeStamp = 0;

        return true;
    }

    function timeOutChannel() public returns (bool) {
        require(channelActive, "payment channel not active");
        require(now > expirationDate, "payment channel not expired yet");

        // increase nonce after payment channel is closed
        customerNonces[channelCustomer] = customerNonces[channelCustomer].add(1);

        // return funds to customer if channel was not closed before channel expiration date
        balances[channelCustomer] = balances[channelCustomer].add(maxValue);

        emit ClosedPaymentChannel(msg.sender, 0, true, now - creationTimeStamp);

        channelActive = false;
        channelCustomer = 0;
        maxValue = 0;
        expirationDate = 0;
        creationTimeStamp = 0;

        return true;
    }

    /** 
     */
    function verifySignature(bytes memory _signature, uint256 _value, uint256 _nonce) public view returns (bool) {

        // split signature into r,s,v values (https://programtheblockchain.com/posts/2018/02/17/signing-and-verifying-messages-in-ethereum/)
        require(_signature.length == 65, "signature length is not 65 bytes");

        bytes32 r;
        bytes32 s;
        uint8 v;

        // split using inline assembly
        assembly {
            // first 32 bytes of message
            r := mload(add(_signature, 32))
            // second 32 bytes of message
            s := mload(add(_signature, 64))
            // first byte of the next 32 bytes
            v := byte(0, mload(add(_signature, 96)))
        }

        // variables that are included in the message: value, address of contract, nonce
        address contractAddress = address(this);
        // hash variables
        bytes32 message = keccak256(abi.encodePacked(_value, contractAddress));
        // prefix message with ethereum specific 
        bytes32 prefixedMessage = keccak256(abi.encodePacked("\x19Ethereum Signed Message:\n32", message));
        // returns true if recovered address is equal to customer address
        return ecrecover(prefixedMessage, v, r, s) == channelCustomer;
    }

    function withdraw() public returns (bool) {
        uint256 balance = balances[msg.sender];
        balances[msg.sender] = 0;
        msg.sender.transfer(balance);
        emit Withdrawal(msg.sender, balance);
        return true;
    }

    function changePrice(uint256 _newPrice) public onlyOwner {
        emit PriceChanged(pricePerSecond, _newPrice);
        pricePerSecond = _newPrice;
    }
}