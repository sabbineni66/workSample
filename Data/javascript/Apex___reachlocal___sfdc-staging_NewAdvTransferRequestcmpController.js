({
    
    showAccountName : function(component, event) {
        var action =component.get("c.getAccountName");
        var accountId = component.get("v.recordId");
        action.setParams({
            
            "accountId":accountId
            
        }) 
        action.setCallback(this, function(response){
            var accountNameState = response.getState();
            if (accountNameState === "SUCCESS") {
                component.set("v.accountName", response.getReturnValue());
            }
            
        });
        $A.enqueueAction(action);
    },
    
    handleCustomBusinessLookupEvent : function(component, event) {
        var businessId = event.getParam("businessId");
        component.set("v.businessId", businessId);
    },
    
    cancelSetUpMeeting:function(component, event, helper){
        $A.get("e.force:closeQuickAction").fire();
    },
    
    saveTransferRequest : function(component, event) {
       var businessId = component.get("v.businessId");
        if(businessId === null){
            component.set("v.showErrorMessage", true);
            component.set("v.showError", 'Please fill BID');    
        }
        var transferId = component.get("v.transfer");
        if(transferId.Transfer_Reason__c === '' ){
            component.set("v.showErrorMessage", true);
            component.set("v.showError", 'Please fill transfer reason');  
        }
        
        if(transferId.Transfer_Reason__c !== '' && businessId !== null){
            component.set("v.showErrorMessage", false);
            var crossplatformassignment = component.get("v.crossplatformassignment");
            var accountId = component.get("v.recordId");
            var businessId = component.get("v.businessId");
            var action =component.get("c.callsaveTransferRequest");
            
            action.setParams({
                
                "accountId":accountId,
                "transferId":transferId,
                "businessId":businessId
                
            }) 
            action.setCallback(this, function(response){
                var transferRequestReturnValue = response.getReturnValue();
                var transferRequeststate = response.getState();
                if (transferRequeststate === "SUCCESS" && transferRequestReturnValue !== crossplatformassignment) {
                    location.reload(); 
                }
                else{
                    component.set("v.showError", transferRequestReturnValue);
                    component.set("v.showErrorMessage", true);
                }
                
            });
            $A.enqueueAction(action);
        }
        
    }
    
})