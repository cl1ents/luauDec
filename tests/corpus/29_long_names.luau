-- method and field names longer than 40 characters are still plain identifiers
local obj = {}
function obj:CreateEditableImageFromBinaryStringRobloxOnly(x)
	return x * 2
end
print(obj:CreateEditableImageFromBinaryStringRobloxOnly(21))

local t = { aVeryLongFieldNameThatIsDefinitelyLongerThanFortyCharacters = 1 }
t.anotherVeryLongFieldNameThatIsDefinitelyLongerThanFortyCharacters = 2
print(t.aVeryLongFieldNameThatIsDefinitelyLongerThanFortyCharacters, t.anotherVeryLongFieldNameThatIsDefinitelyLongerThanFortyCharacters)
